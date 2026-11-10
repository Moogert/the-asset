# Code architecture

## Execution boundaries

| Component | Runs on | Current authority |
|---|---|---|
| C simulator and web UI | Desktop | Synthetic CAN and simulated state only |
| C++03 supervisor/runtime | Host tests; portable STM32F107 objects | Policy and abstract ports; no completed physical VCU port |
| Mega firmware | Mega 2560 review build | Engine-signal scheduler; unverified Honda outputs disabled |
| Teensy firmware | Teensy 4.1 review build | EV records and emulator coordination; original Honda ECU owns OBD/MIL |
| Pawl prototypes | Uno review builds | Reference interlocks; actuator outputs disabled |
| Python bench tools | Desktop | Import/analysis/reporting only; no live hardware commands |

## Source ownership

`src/core` and `include` hold the C reference simulator. `src/control` and
`include/control` implement the separate portable C++ policy/runtime. Tests compare
reference behavior and exercise thermal, dispatch, feedback and fault handling.
The application demos use mock hardware; the UI continues to use the C simulator.

`firmware/lib/CapstoneIO/src` is shared by host tests and board builds. Electrical
bindings live in `firmware/include`; transport-specific applications live in
`firmware/src`. The isolated legacy OBD example is excluded from default builds.

Each `firmware/parking_pawl` sketch has its own project and `setup()`/`loop()`;
these are independent of the Mega/Teensy build. Their exact driver/MCU allocation
and vehicle interlocks remain review work.

`tools/benchkit` owns normalized recorded data, experiment analysis and reports.
Other tools calculate drivetrain/thermal/pack requirements, export project
connectors, or run optional external review tools. Configurations separate
synthetic envelopes from measured inputs, which remain unset until verified.

## Version-one dependency boundary

The MG2-only Gen 2 MVP requires actual controller identification, verified pedal,
pack/HV/brake/cooling interfaces and MG1 passive/back-drive safety. It does not
require Gen 3/V1d dual-channel dispatch. Existing dual-MG code is an upgrade
prototype, not a validated driver for the purchased Gen 2 controller.
