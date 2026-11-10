> Historical architecture, superseded 29 October 2026. Do not use as the vehicle wiring specification.

# Project EV diagnostic code dictionary

28 October 2026. **Project-defined codes, not assigned Honda descriptions.**
The reader may supply a different manufacturer description; use the numeric code.
Source IDs: gateway 0, legacy supervisor 1, C++ runtime 2, Honda 3, MG1 4, MG2 5.

| Code | Source | Project meaning |
| --- | --- | --- |
| P1F00 | Gateway | EV telemetry unavailable or stale |
| P1F01 | Gateway | Honda diagnostic polling unavailable or incomplete |
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
| P1F50 | C++ runtime bit 16 | Runtime configuration |
| P1F51 | C++ runtime bit 17 | Runtime deadline |
| P1F52 | C++ runtime bit 18 | BMS freshness/validity |
| P1F53 | C++ runtime bit 19 | Control transport |
| P1F54 | C++ runtime bit 20 | Effort mapping |
| P1F55 | C++ runtime bit 21 | Inhibit driver |
| P1F60 | MG1 | Source reports MG1 channel fault |
| P1F61 | MG2 | Source reports MG2 channel fault |

MG1/MG2 channel codes are issued only when the publisher supplies channel identity.
An aggregate thermal or feedback fault cannot identify the affected motor by itself.
No insulation, cell-level battery or pump-specific code is fabricated from missing data.

Original Honda codes retain their numeric values and use Honda service definitions.
If an original code collides with this project namespace, source records remain separate
over USB but the generic code list deduplicates the number; resolve the mapping collision
before claiming reader acceptance. A generic code has no additional source field.

The gateway warning policy includes active and retained faults. Pending/stored behavior
is a documented project policy, not an OEM emissions-monitor implementation.
