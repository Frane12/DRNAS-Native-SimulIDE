#!/usr/bin/env python3
"""Small, checked adaptations against the locked upstream source; safe to rerun."""
from pathlib import Path
import sys
import shutil
import xml.etree.ElementTree as ET

root = Path(sys.argv[1]).resolve()
pri = root / 'SimulIDE.pri'
text = pri.read_text()
marker = '# DRNAS Android build adaptations'
if marker not in text:
    start = text.index('runLrelease.commands =')
    end = text.index('\nmessage(', start)
    text = text[:start] + '# Translations are generated explicitly by the pipeline.\n' + text[end:]
    text += '''
# DRNAS Android build adaptations
android {
    OS = Android
    ANDROID_PACKAGE_SOURCE_DIR = $$PWD/android
    ANDROID_ABIS = arm64-v8a
    ANDROID_MIN_SDK_VERSION = 23
    ANDROID_TARGET_SDK_VERSION = 29
    ANDROID_VERSION_NAME = 0.1.0
    ANDROID_VERSION_CODE = 1
    # Include ARM64 call glue even though the build host is x86_64.
    SOURCES *= $$PWD/src/angel/src/as_callfunc_arm64_gcc.S
    DESTDIR = $$OUT_PWD/lib
}
'''
    pri.write_text(text)

# Android has no POSIX shm_open backend or desktop QEMU subprocess.
# Keep upstream's existing "no arena" failure path; never link Linux shm APIs.
qemu = root / 'src/microsim/cores/qemu/qemudevice.cpp'
text = qemu.read_text()
text = text.replace('#ifdef __linux__', '#if defined(__linux__) && !defined(__ANDROID__)')
qemu.write_text(text)

# Fail early if the pinned source contains missing resources. .qm files are
# generated later on the Qt-equipped runner, from the corresponding .ts files.
qrc = root / 'src/application.qrc'
files = list(ET.parse(qrc).iter('file'))
for item in files:
    path = (qrc.parent / item.text).resolve()
    if path.suffix == '.qm':
        candidates = list((root / 'resources/translations').rglob(path.stem + '.ts'))
        if not candidates:
            raise SystemExit(f'Missing translation source: {path.name}')
    elif not path.is_file():
        raise SystemExit(f'Missing qrc resource: {path}')
print(f'Checked {len(files)} resources; Android assembly and QEMU guards applied.')

# Include upstream runtime data in the APK and provision it to the same private
# config directory that the original component/help loaders already use.
assets = root / 'android/assets/data'
shutil.copytree(root / 'resources/data', assets, dirs_exist_ok=True)
main = root / 'src/main.cpp'
text = main.read_text()
marker = '// DRNAS Android runtime data'
if marker not in text:
    text = text.replace('#include <QApplication>', '#include <QApplication>\n#include <QDirIterator>')
    anchor = '    QApplication app( argc, argv );'
    assert text.count(anchor) == 1
    bootstrap = r"""
#ifdef Q_OS_ANDROID
    // DRNAS Android runtime data
    const QString assetRoot = "assets:/data";
    const QString privateRoot = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(privateRoot);
    QDirIterator assets(assetRoot, QDir::Files, QDirIterator::Subdirectories);
    while (assets.hasNext()) {
        const QString source = assets.next();
        const QString relative = source.mid(assetRoot.size() + 1);
        const QString target = privateRoot + "/data/" + relative;
        QDir().mkpath(QFileInfo(target).absolutePath());
        if (!QFile::exists(target) && !QFile::copy(source, target))
            qWarning() << "Cannot provision SimulIDE resource" << relative;
    }
#endif
"""
    text = text.replace(anchor, anchor + bootstrap)
    main.write_text(text)
print(f'Packaged {sum(p.is_file() for p in assets.rglob("*"))} runtime data assets.')

# Keep UI adaptations separate from source/dependency preparation.
import subprocess
subprocess.run([sys.executable, str(Path(__file__).with_name('apply_mobile.py')), str(root)], check=True)
