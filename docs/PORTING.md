# Android porting checklist

1. Select and pin a SimulIDE upstream commit.
2. Inventory source tree, build system, Qt modules, AVR/PIC simulation backends, and resource paths.
3. Identify desktop-only APIs (serial ports, file dialogs, process launching, plugin loading).
4. Configure Qt Android kit with Android SDK/NDK and arm64-v8a ABI.
5. Compile upstream core with minimal changes, recording failures.
6. Integrate original GUI and test touch/focus/graphics.
7. Configure storage through Android-compatible paths and SAF.
8. Package assets and verify real circuit simulation on device.

## Acceptance tests

- App opens without abort.
- Components palette loads.
- Place voltage source, resistor and LED.
- Connect and run simulation.
- Save, reopen and verify the circuit.

No acceptance test has passed yet.
