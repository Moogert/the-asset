# Parking-pawl review projects

| Project | Role |
|---|---|
| [PriusParkPawlBenchController](PriusParkPawlBenchController/) | Guarded actuator characterization draft |
| [PriusParkPawlVehicleController](PriusParkPawlVehicleController/) | Hardwired interlock prototype; incomplete vehicle release policy |

Each folder is a separate Uno/AVR PlatformIO project and matching Arduino sketch.
Both set `ENABLE_ACTUATOR_OUTPUTS = false`. Follow the
[integration review](../../docs/parking-pawl/INTEGRATION_REVIEW.md) and
[build commands](../../docs/DEVELOPMENT.md).

The original publication is commit `70506a0`; its immutable source manifest and
compile record remain under `docs/parking-pawl`. This folder move does not change
the published sketches' source logic. No vehicle acceptance is claimed.
