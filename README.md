# THE ASSET — Prius parking-pawl development

Gen 2 Prius parking-lock actuator reference code for the 1996 Civic DX EV project.
This first publication contains the parking-pawl package only.

**Status: development drafts; actuator outputs disabled by default.** Compilation
does not establish correct wiring, motion, stopping behavior or vehicle suitability.
The Civic was originally manual: automatic-shifter inputs in the inherited vehicle
sketch must be replaced by a reviewed park/release request interface.

## Contents

| Sketch | Purpose |
|---|---|
| [Bench controller](arduino_prius_parking_pawl/PriusParkPawlBenchController/PriusParkPawlBenchController.ino) | Serial characterization of phase direction, current, Hall activity and travel |
| [Vehicle draft](arduino_prius_parking_pawl/PriusParkPawlVehicleController/PriusParkPawlVehicleController.ino) | Prototype hardwired interlocks, position checks and motion-fault handling |

[Integration review](docs/INTEGRATION_REVIEW.md) distinguishes implemented checks
from required work. [Inherited actuator notes](docs/LEGACY_ACTUATOR_NOTES.md) and
[CAN notes](docs/LEGACY_CAN_NOTES.md) retain earlier research; their wiring,
ratings and automatic-Civic assumptions require verification.

## Reproduce the builds

Install PlatformIO Core **6.1.18** in a Python virtual environment. Each sketch has
its own project so Arduino does not combine multiple `setup()`/`loop()` functions.

```sh
python -m pip install platformio==6.1.18
python -m platformio run -d arduino_prius_parking_pawl/PriusParkPawlBenchController
python -m platformio run -d arduino_prius_parking_pawl/PriusParkPawlVehicleController
```

The pinned Uno/AVR builds verify compilation, not a selected final vehicle MCU.
Arduino IDE can open each matching sketch folder separately. No upload command is
part of this review workflow.

## Before any actuator connection

`ENABLE_ACTUATOR_OUTPUTS` is `false` in both sketches: motor-feed enable and phase
PWM remain off. Changing that flag is not sufficient to authorize operation.
Review the exact actuator/harness, external power driver, current sensing,
inductive protection, independent cutoff and guarded test procedure first.

Retain the mechanical parking brake. Never command park while moving or infer
actual pawl position from a request, Hall edge count or loss of electrical power.
No hardware operation, installation acceptance or full JSF compliance is claimed.
