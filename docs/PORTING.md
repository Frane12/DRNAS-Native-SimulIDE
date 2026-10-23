# SimulIDE Android ARM64 pipeline

Run **SimulIDE ARM64 dependencies to APK** from Actions → Run workflow on `main`.
Pushes touching the pipeline or scripts also start it; pull requests validate the
same sequence. No manual hand-off between dependency, compiler and APK stages.

## Ordered gates

1. Source preflight: fetch the exact upstream SHA, check resource references and
   apply repeatable Android adaptations. A failure prevents the build job.
2. Dependencies: install locked SDK/NDK and Qt 5.15.2's multi-ABI `android` kit.
   Qt's standard archives are not requested as optional modules. If SerialPort,
   SVG or Multimedia is absent, build that module from locked matching Qt source.
   Qt cache keys include the lock and dependency script; restored caches still
   undergo the compiler/linker probe.
3. Toolchain preflight: require SDK, NDK, Qt tools, and compile/link a small
   ARM64 library against all required Qt modules; inspect its actual ELF header.
4. Compile: generate translations explicitly, build the original application
   for `arm64-v8a`, require the application ELF and deployment JSON.
5. Package: stage the real native products and run `androiddeployqt` with the
   pinned Android SDK platform and Java 8, producing a signed debug APK.
6. Audit: verify the signature, ZIP contents, manifest/DEX/resources, required
   Qt runtime libraries, every native ELF's AArch64 architecture, and all
   DT_NEEDED dependencies against APK libraries plus the NDK API system stubs.
7. Upload: only the audited APK enters `dist/`, alongside SHA256, provenance,
   audit output and an explicit statement that device tests have not run.

Each stage fails closed. Logs are uploaded even on a failed build. A source
compile error still requires a code fix; the workflow does not suppress errors
or claim success because it produced only diagnostic files. GitHub's **Re-run
failed jobs** retries the build job; dependency caches reduce repeated downloads.

All versions and source SHAs are in `scripts/android.env`. The Linux x86_64 host
runs build tools; the application's only target ABI is Android ARM64.
`sdkmanager` uses the runner's Java 17, while Qt 5.15.2's Gradle uses Java 8.
This is an experimental debug build, not a Play Store release configuration.

## Android adaptations and remaining device work

- ARM64 AngelScript assembly is included explicitly during cross compilation.
- Qt resources and translations are compiled into the application. Upstream
  runtime data is bundled as APK assets and copied into private app storage on
  first launch without overwriting user files.
- Desktop QEMU uses POSIX shared memory and external QEMU processes; Android
  cannot use that implementation. Its Linux-only branches are excluded and the
  upstream unavailable-backend path remains. This does **not** claim STM32/ESP32
  QEMU support. The built-in simulation cores remain in the source build.
- Hardware serial access, external firmware compilers, SAF/storage, touch layout
  and real circuit simulation need device validation. Building QtSerialPort does
  not grant access to Android USB devices.

## Device acceptance tests (none confirmed yet)

- App opens without abort and components palette loads.
- Place voltage source, resistor and LED; connect and run simulation.
- Save and reopen the circuit in an Android-accessible location.
- Test built-in MCU simulation separately from unavailable desktop QEMU.

## Mobile display build 0.2.0

- Launcher name: **DRNAS SimulIDE**. The adaptive icon is a redesigned derivative
  of the upstream circuit-board mark; the opening screen says **DRNAS EMU** and
  **SimulIDE**. The Android package and private data path stay compatible with
  the previous build.
- Immersive fullscreen hides status/navigation bars. Android edge swipes can
  reveal system controls. Camera cutout insets protect the app's controls.
- Android owns rotation without Activity recreation; the circuit remains loaded.
  The components/files dock moves left in landscape and below in portrait.
- The **Components**, **Circuit**, **Editor** and **Output** controls replace the
  cramped three-column startup arrangement. The editor and output start closed.
- Font scale is 1.35 in logical pixels, toolbar icons are 30 px, and main touch
  targets are 44 px or larger. Qt handles device density independently.
- Start/pause and +/− zoom precede the remaining circuit toolbar actions. A
  two-finger pinch also zooms, and rotation keeps the viewport's center.

Validation must include portrait ↔ landscape, reopening from background, showing
and dismissing the keyboard, opening/closing all four mobile controls, and
checking that the camera cutout does not cover buttons. These require a device.
