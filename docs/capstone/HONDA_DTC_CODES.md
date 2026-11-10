# Project EV diagnostic code dictionary

29 October 2026. **Internal EV record identifiers, not codes published to vehicle OBD.**
The original Honda ECU supplies reader codes from verified induced signal conditions.
Source IDs: gateway 0, legacy supervisor 1, C++ runtime 2, Honda 3, MG1 4, MG2 5.

| Code | Source | Project meaning |
| --- | --- | --- |
| P1F00 | Gateway | EV telemetry unavailable or stale |
| P1F01 | Legacy demonstration only | Honda polling unavailable; default image does not poll Honda |
| P1F02 | Gateway | Engine emulator unavailable or faulted |
| P1F03 | Gateway | Diagnostic persistence failure |
| P1F04 | Gateway | Gateway timing / diagnostic transmit failure |
| P1F05 | Gateway | Unknown telemetry fault bits |
| P1F06 | Gateway | Unverified vehicle interface profile |
| P1F07 | Gateway | Fault-table capacity exceeded |
| P1F10 | Legacy supervisor bit 0 | BMS stale |
| P1F11 | Legacy supervisor bit 1 | Inverter stale |
| P1F12 | Legacy supervisor bit 2 | Charger stale |
| P1F13 | Legacy supervisor bit 3 | Control bus off |
| P1F14 | Legacy supervisor bit 4 | Invalid control frame |
| P1F15 | Legacy supervisor bit 5 | BMS limit |
| P1F16 | Legacy supervisor bit 6 | Inverter fault |
| P1F17 | Legacy supervisor bit 7 | Overcurrent |
| P1F18 | Legacy supervisor bit 8 | Overspeed |
| P1F19 | Legacy supervisor bit 9 | HV interlock |
| P1F1A | Legacy supervisor bit 10 | Emergency stop |
| P1F1B | Legacy supervisor bit 11 | Contactor/precharge fault |
| P1F1C | Legacy supervisor bit 12 | Pedal plausibility |
| P1F1D | Legacy supervisor bit 13 | Charger fault |
| P1F1E | Legacy supervisor bit 14 | Contradictory request |
| P1F1F | Legacy supervisor bit 15 | Supervisor configuration |
| P1F40 | C++ runtime bit 0 | Controller configuration |
| P1F41 | C++ runtime bit 1 | Motor feedback stale |
| P1F42 | C++ runtime bit 2 | Invalid motor feedback |
| P1F43 | C++ runtime bit 3 | Motor channel fault |
| P1F44 | C++ runtime bit 4 | Motor overspeed |
| P1F45 | C++ runtime bit 5 | Motor thermal fault |
| P1F46 | C++ runtime bit 6 | Pack current/limit fault |
| P1F47 | C++ runtime bit 7 | Motor circulation |
| P1F48 | C++ runtime bit 8 | Drive permission |
| P1F49 | C++ runtime bit 9 | Scheduler fault |
| P1F4A | C++ runtime bit 10 | Gearset kinematics |
| P1F4B | Release bit 11 | Required component measurement unavailable |
| P1F50 | C++ runtime bit 16 | Runtime configuration |
| P1F51 | C++ runtime bit 17 | Runtime deadline |
| P1F52 | C++ runtime bit 18 | BMS freshness/validity |
| P1F53 | C++ runtime bit 19 | Control transport |
| P1F54 | C++ runtime bit 20 | Effort mapping |
| P1F55 | C++ runtime bit 21 | Inhibit driver |
| P1F56 | Release bit 22 | Dual-channel transmission timing failure |
| P1F60 | MG1 | Source reports MG1 channel fault |
| P1F61 | MG2 | Source reports MG2 channel fault |

MG1/MG2 channel codes are issued only when the publisher supplies channel identity.
An aggregate thermal or feedback fault cannot identify the affected motor by itself.
No insulation, cell-level battery or pump-specific code is fabricated from missing data.

## Printable Honda mapping worksheet

No Honda category code is verified yet. Complete [the mapping worksheet](../../configs/honda-fault-indication.json)
from the identified ECU, scope evidence and reader results. Record code, prerequisites,
detection delay, recovery and simultaneous-condition behavior.

| Category bit | EV category | Observed Honda code |
| --- | --- | --- |
| 0 | Thermal | Pending ECU test |
| 1 | Circulation | Pending ECU test |
| 2 | Feedback | Pending ECU test |
| 3 | Channel/speed | Pending ECU test |
| 4 | Battery/permission | Pending ECU test |
| 5 | Communication | Pending ECU test |
| 6 | Timing | Pending ECU test |
| 7 | Configuration/unknown source bits | Pending ECU test |

The reader retains its Honda sensor/circuit description. These project interpretations
are not official Honda code assignments. A real circuit fault can produce the same
code, and several simultaneous stimuli may not produce independently readable codes.
Use USB/Bluetooth detail to identify every actual EV fault and affected component.
Legacy C masks remain recorded completely; their category fallback is configuration
until a separate semantic translation is verified. OBD detection has no traction
shutdown responsibility. Native Honda code clearing cannot rearm traction.
