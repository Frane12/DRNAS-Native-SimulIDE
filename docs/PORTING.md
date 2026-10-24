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

## Mobile 0.3.0 — startup display and dismissible windows

The Android startup dialog appears before QApplication creates its screens. It
shows the estimated logical workspace dimensions and a 60–140% scale slider,
with **Apply and open**, **Use saved**, and **Automatic**. The selected scale is
applied through QT_SCALE_FACTOR for this launch, while Android retains its
physical screen mode and aspect ratio. The camera-safe-area option can remove
the reserved top strip; disabling it may put controls near a camera cutout.
The setting is persisted in private Android preferences.

All Qt dialogs receive a visible 120×48 **× Close** button. Closing still uses
upstream closeEvent handling. The components dock has a labelled **× Close**
button, **Editor** toggles back to the circuit, **Output** is dismissible with its
toggle, and **× Close panels** returns to an unobstructed circuit view.
Android 11+ immersive mode also uses WindowInsetsController when available.

Device checks: change scale at startup; cancel and confirm display selection;
rotate with an unsaved circuit; open/close settings, component properties and
instruments; open/close editor/output/components; compare safe-area modes and
verify a saved circuit remains intact. Compilation and APK audits do not verify
these interactions.

## Mobile 0.4.0 — touch wiring

Android picks the closest eligible free pin within 22 viewport logical pixels.
The radius is independent of scene zoom. The selected pin gets a cyan ring,
a pin-name message and Android haptic feedback (subject to system settings).
Component-body dragging remains the normal path when a body is pressed.

Either tap the start and destination pins, or drag from a start pin and release
on the highlighted destination. Releasing a source tap leaves the wire armed;
it cannot silently choose an adjacent pin. The source pin, occupied/unused pins
and incompatible bus pins are excluded from destination selection. Empty-space
taps can still place route corners. Pin geometry and electrical collision /
auto-connect rules remain upstream's originals.

**Cancel wire** is ahead of zoom in the circuit toolbar and enabled only during
an unfinished wire. Android Back / Escape can also cancel it. Both call the
existing deleteNewConnector/cancelUndoStep path. Completed wires are unaffected.
Pinch zoom suppresses the pending one-finger release so it cannot finish a wire.

Device test: tap–tap; drag–release; source tap near dense IC pins; miss a target
and retry; cancel after one or more corners; try occupied and incompatible bus
pins; zoom while drawing; rotate and repeat. Verify existing connections remain
intact after cancel. APK/static checks do not establish touch behavior on device.

## Mobile 0.5.0 — two-finger camera and temporary wire messages

The existing Qt pinch recognizer supplies both scale and midpoint motion. Moving
both fingers together now pans the circuit view, while spreading/pinching zooms
around their midpoint. Combined motion preserves the scene point under the
fingers. Rotation rebases the gesture anchor. Completed circuit components and
connections are not moved by this camera operation.

A two-finger gesture releases the previous item grab and suppresses pending
wire mouse releases. The unfinished wire remains available for continuing or
Cancel. A touch-cancel/new touch sequence resets camera gesture state.

Wire feedback is now a fixed overlay on the view rather than a scrolling child
of its viewport. A single restartable 1.5-second timer hides every message,
including wires completed through the original junction path. Messages are
shorter; beginning camera motion hides feedback immediately. Pin highlighting
still remains available during active finger selection.

The shared camera math is tested in the source preflight for pure pan, unchanged
camera, combined pan/zoom anchor preservation, inverse moves and scale limits.
Device checks: pan horizontally/vertically with two fingers, combine pan and
zoom, lift/restart fingers, rotate, and repeat while a wire is unfinished. Ensure
components remain in place, wiring/cancel still work and no notification sticks.


## Mobile 0.6.0 — compact portrait controls and full display fit

The mobile navigation uses 22px icons, 11px captions and shorter Parts/Close
labels. Circuit controls reuse upstream actions in an adaptive grid with 40px
high touch targets: run, pause, cancel wire, zoom +/−, fit circuit, fit selection,
and More. At narrow widths they wrap into balanced rows instead of disappearing
into toolbar overflow. Files, settings, recent circuits, 1:1 zoom and help remain
in More. Width is recalculated on rotation and components dock visibility changes.
The desktop toolbar remains intact but hidden on Android. Its simulation/error
status label is retained below the controls and appears only when nonempty.

Startup defaults to Fit entire screen, including the camera area, and does not
reuse the old default safe-area reservation. This removes the explicit top
cutout margin. Turning off Fit entire screen restores optional safe margins;
scale and display fit are saved independently. Immersive mode also disables
framework fitsSystemWindows padding.

Device checks: portrait/landscape at 60%, 100%, 140% display scale; verify all
primary controls are visible, menus open and simulation/cancel actions retain
state. Open/close the components dock, rotate with an unfinished wire and check
both display-fit settings. Confirm the top strip on the actual phone is gone.
