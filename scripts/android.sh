#!/usr/bin/env bash
set -Eeuo pipefail
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$ROOT"
source scripts/android.env
mkdir -p logs
trap 'printf "::error::Phase %s failed at line %s\n" "${1:-unknown}" "$LINENO"' ERR
export ANDROID_SDK_ROOT=${ANDROID_SDK_ROOT:-${ANDROID_HOME:-}}
export ANDROID_HOME=$ANDROID_SDK_ROOT
export ANDROID_NDK_ROOT=$ANDROID_SDK_ROOT/ndk/$NDK_VERSION
export ANDROID_NDK_HOME=$ANDROID_NDK_ROOT
QT_ROOT=$ROOT/Qt/$QT_VERSION/android
export PATH=$QT_ROOT/bin:$PATH
READELF=$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-readelf

fetch_source() {
    local url=$1 sha=$2 dest=$3
    if [[ ! -d $dest/.git ]]; then git init "$dest"; git -C "$dest" remote add origin "$url"; fi
    git -C "$dest" fetch --depth 1 origin "$sha"
    git -C "$dest" checkout --detach FETCH_HEAD
    [[ $(git -C "$dest" rev-parse HEAD) == "$sha" ]]
}

case ${1:?phase required} in
source)
    fetch_source https://github.com/Arcachofo/SimulIDE-dev.git "$UPSTREAM_SHA" upstream
    test -f upstream/build_XX/SimulIDE_Build.pro
    test -f upstream/src/angel/src/as_callfunc_arm64_gcc.S
    python3 scripts/prepare_source.py upstream
    printf 'Upstream: %s\nAndroid ABI: %s\nPipeline: %s\n' "$UPSTREAM_SHA" "$ANDROID_ABI" "${GITHUB_SHA:-local}" > logs/source.txt
    ;;
dependencies)
    test -n "$ANDROID_SDK_ROOT"
    # Avoid yes|sdkmanager under pipefail (yes exits with SIGPIPE).
    printf 'y\n' > logs/sdk-license-input.txt
    JAVA_HOME="${JAVA_HOME_17_X64:?Java 17 required for sdkmanager}" \
      "$ANDROID_SDK_ROOT/cmdline-tools/latest/bin/sdkmanager" \
      --sdk_root="$ANDROID_SDK_ROOT" "ndk;$NDK_VERSION" \
      "platforms;android-$ANDROID_API" "build-tools;$BUILD_TOOLS_VERSION" < logs/sdk-license-input.txt
    if [[ ! -x $QT_ROOT/bin/qmake ]]; then
        python3 -m aqt list-qt linux android --arch "$QT_VERSION" | tee logs/qt-architectures.txt
        grep -qw "$QT_ARCH" logs/qt-architectures.txt
        # Qt 5.15 ships one multi-ABI 'android' package. Standard modules are
        # base archives, not optional --modules names.
        python3 -m aqt install-qt linux android "$QT_VERSION" "$QT_ARCH" -O "$ROOT/Qt"
    fi
    # QtSerialPort may be absent in official Android kits. Build missing
    # modules from exactly matching Qt source, never copy host libraries.
    for entry in "serialport:qtserialport:$QTSERIALPORT_SHA" "svg:qtsvg:$QTSVG_SHA" "multimedia:qtmultimedia:$QTMULTIMEDIA_SHA"; do
        IFS=: read -r module repo sha <<< "$entry"
        if [[ ! -f $QT_ROOT/mkspecs/modules/qt_lib_$module.pri ]]; then
            fetch_source "https://github.com/qt/$repo.git" "$sha" "deps/$repo"
            mkdir -p "deps/build-$repo"
            (
                cd "deps/build-$repo"
                "$QT_ROOT/bin/qmake" "$ROOT/deps/$repo/$repo.pro" \
                  -spec android-clang ANDROID_ABIS="$ANDROID_ABI" \
                  ANDROID_PLATFORM="android-$ANDROID_API" ANDROID_SDK_BUILD_TOOLS_REVISION="$BUILD_TOOLS_VERSION" CONFIG+=release CONFIG-=debug
                make -j2
                make install
            )
        fi
    done
    ;;
toolchain)
    for tool in qmake moc rcc lrelease androiddeployqt; do test -x "$QT_ROOT/bin/$tool"; done
    test "$(qmake -query QT_VERSION)" == "$QT_VERSION"
    test -f "$ANDROID_NDK_ROOT/source.properties"
    grep -q "$NDK_VERSION" "$ANDROID_NDK_ROOT/source.properties"
    test -x "$ANDROID_SDK_ROOT/build-tools/$BUILD_TOOLS_VERSION/apksigner"
    test -f "$ANDROID_SDK_ROOT/platforms/android-$ANDROID_API/android.jar"
    # Actually compile and link against every required module before the big build.
    mkdir -p build/probe
    printf '%s\n' 'QT += core gui widgets xml concurrent network svg multimedia serialport' \
      'TEMPLATE = lib' 'CONFIG += shared' 'TARGET = dependency_probe' 'SOURCES += probe.cpp' > build/probe/probe.pro
    printf '%s\n' '#include <QSerialPort>' '#include <QSvgRenderer>' '#include <QAudioOutput>' \
      'extern "C" int probe() { QSerialPort p; QSvgRenderer s; return p.isOpen() + s.isValid(); }' > build/probe/probe.cpp
    (cd build/probe; qmake probe.pro -spec android-clang ANDROID_ABIS="$ANDROID_ABI" ANDROID_PLATFORM="android-$ANDROID_API" ANDROID_SDK_BUILD_TOOLS_REVISION="$BUILD_TOOLS_VERSION"; make -j2)
    probe=$(find build/probe -name 'libdependency_probe*.so' -print -quit)
    test -n "$probe"
    "$READELF" -h "$probe" | tee logs/toolchain-elf.txt
    grep -q AArch64 logs/toolchain-elf.txt
    ;;
compile)
    test -f logs/source.txt
    test -f logs/toolchain-elf.txt
    mkdir -p build upstream/resources/qm
    while IFS= read -r -d '' ts; do
        lrelease "$ts" -qm "upstream/resources/qm/$(basename "${ts%.ts}").qm"
    done < <(find upstream/resources/translations -name '*.ts' -print0)
    (cd build; qmake ../upstream/build_XX/SimulIDE_Build.pro \
      -spec android-clang ANDROID_ABIS="$ANDROID_ABI" ANDROID_PLATFORM="android-$ANDROID_API" ANDROID_SDK_BUILD_TOOLS_REVISION="$BUILD_TOOLS_VERSION" \
      CONFIG+=debug CONFIG-=release; make -j2)
    lib=$(find build upstream/build_XX -name 'libsimulide_arm64-v8a.so' -print -quit)
    test -n "$lib"
    "$READELF" -h "$lib" | tee logs/simulide-elf.txt
    grep -q AArch64 logs/simulide-elf.txt
    test -f build/android-simulide-deployment-settings.json
    ;;
package)
    test -f logs/simulide-elf.txt
    test -f build/android-simulide-deployment-settings.json
    mkdir -p build/android-build
    (cd build; make INSTALL_ROOT="$ROOT/build/android-build" install)
    androiddeployqt --input "$ROOT/build/android-simulide-deployment-settings.json" \
      --output "$ROOT/build/android-build" --android-platform "android-$ANDROID_API" \
      --jdk "$JAVA_HOME" --verbose
    mapfile -t apks < <(find build/android-build/build/outputs/apk -name '*debug.apk' -type f)
    [[ ${#apks[@]} == 1 ]]
    mkdir -p candidate
    cp "${apks[0]}" candidate/DRNAS-SimulIDE-Android-ARM64-debug.apk
    ;;
verify)
    apk=candidate/DRNAS-SimulIDE-Android-ARM64-debug.apk
    test -s "$apk"
    "$ANDROID_SDK_ROOT/build-tools/$BUILD_TOOLS_VERSION/apksigner" verify --verbose "$apk" | tee logs/signature.txt
    "$ANDROID_SDK_ROOT/build-tools/$BUILD_TOOLS_VERSION/aapt" dump badging "$apk" | tee logs/apk-badging.txt
    python3 scripts/verify_apk.py "$apk" "$READELF" \
      "$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/lib/aarch64-linux-android/$ANDROID_API" | tee logs/apk-audit.txt
    mkdir -p dist
    cp "$apk" dist/
    sha256sum dist/*.apk > dist/SHA256SUMS.txt
    printf 'SimulIDE experimental Android debug APK\nABI: %s\nQt: %s\nNDK: %s\nUpstream: %s\nPipeline: %s\nSignature and ELF dependency audit: PASS\nDevice acceptance tests: NOT RUN\nDesktop QEMU shared-memory backend: unavailable on Android\n' \
      "$ANDROID_ABI" "$QT_VERSION" "$NDK_VERSION" "$UPSTREAM_SHA" "${GITHUB_SHA:-local}" > dist/build-info.txt
    cp logs/apk-audit.txt dist/
    ;;
*) printf 'Unknown phase: %s\n' "$1" >&2; exit 2 ;;
esac
