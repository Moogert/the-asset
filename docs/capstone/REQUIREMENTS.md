# Requirements and verification matrix

Status vocabulary: implemented/tested = local software only; pending = no hardware evidence.

| ID | Requirement | Verification/evidence | Current status |
| --- | --- | --- | --- |
| R01 | MG2-only baseline, MG1-only and combined modes | C mode tests; loaded separate runs | Software tested; load tests pending |
| R02 | Both channels fresh and healthy | Per-channel timeout/reset/resolver tests, source/CRC/sequence validation | Simulation tested; real mappings pending |
| R03 | Shared discharge and regen allocation | Request/budget sweep plus BMS zero-limit tests | Simulation tested; real current calibration pending |
| R04 | Stale feedback inhibits both by 100 ms | Exact boundary test; independent hardware latency capture | Software tested; physical inhibit pending |
| R05 | No automatic restart after fatal fault | Fault-removal and disarmed-reset tests | Software tested; hardware pending |
| R06 | Faults include bus-off, pedal, BMS, HVIL, emergency stop, feedback and overcurrent | Legacy and dual C regressions; commissioning B/E | Software tested; hardware pending |
| R07 | Speed/temperature and locked-gear plausibility | Signed overspeed, thermal derating, unequal speed tests | Simulation tested; limits uncalibrated |
| R08 | No sustained unintended circulating power | Synchronized uncertainty-aware opposing-power confirmation plus loaded captures | Simulation tested; sensor/physical validation pending |
| R09 | Combined torque improves beyond uncertainty | Matched loaded CSV comparison; calibrated fixture | Analysis tool tested using synthetic fixtures; measurements pending |
| R10 | 75 mph for twenty minutes on a closed course | Updated 75 mph study, measured loaded power, energy and logged endurance/temperature evidence | Calculated scenarios only; hills required; actual grade/tolerance, hardware and thermal acceptance pending |
| R11 | Traceable firmware/configurations | Upstream lock and per-motor commissioning worksheets | Provenance recorded; no flash/config validation |
| R12 | Reviewed planetary lock, CAN and inhibit hardware | As-built drawings, reviews and inspection records | Pending exact hardware |
| R13 | Precharge/contactor/energy management | Voltage traces, auxiliary feedback, approved failure sequence | Pending hardware; simulator flag is not a sequencer |
| R14 | Vehicle installation | Mounts/axles/braking/parking/battery enclosure inspection | Pending vehicle work |
| R15 | JSF C++ extra-credit evidence for agreed embedded scope | Actual C++ build, compilation database, literal checker report, complete manual rule disposition and justified deviations | C++03 port tested; 40 automated checks report no violations; 193 manual reviews and scope/platform acceptance pending |
| R16 | Porsche conversion fits actual DX knuckles and 15-inch wheels | Correct small/large-hub kit, supplier instructions, rotor/pad/hose/clearance record | Work package and BOM integrated; measurements pending |
| R17 | Complete friction braking with planned Integra rear discs and no ABS | Master bore/stroke, wheel-cylinder/piston areas, proportioning, split circuits, axle weights and no-regen stop/fade/bias tests | Currently drums/no ABS; Integra rear BOM added, compatibility and performance pending |
| R18 | Engine-independent brake assist and parking function | Standalone-rated vacuum system, reserve/recovery/fault tests, warning and mechanical parking-brake inspection | Specified and budgeted; system not selected or validated |

## Acceptance report outline

1. Hardware identities and as-built configuration, source and parameter hashes.
2. Calibration instruments, methods, uncertainty and fixture review.
3. Individual motor results, signs, current readings and resolver plots.
4. Combined loaded torque comparison at matched operating conditions.
5. Battery power/current allocation and regeneration behavior.
6. Fault list, timing traces, inhibit behavior and recovery evidence.
7. Speed-voltage and continuous thermal envelope; road-load comparison.
8. Deviations, failures, remaining limits, and vehicle work still required.
9. Vehicle brake conversion, hydraulic/bias/assist and regen-unavailable braking evidence before road release.

Use `hardware-test-record.csv` for the evidence index. Empty records are intentional;
no simulated test or calculated example is hardware acceptance evidence.

## Battery packaging additions — 28 October 2026

| ID | Requirement | Verification / status |
| --- | --- | --- |
| R19 | Prefer rear-seat retention with fuel tank removed | Tank-cavity/seat-retaining mock-up, occupant/belt clearances, loaded suspension and ground clearance; pending |
| R20 | Justify battery architecture on a common mission | Usable energy, installed mass, donor health, cost, power/thermal and validation comparison; example study documented |
| R21 | Secure and isolate battery installation | Reviewed mounts/enclosure, GVWR/GAWR, restraint, insulation, venting, interlocks, HV protection and service access; pending |

## Original electronics and diagnostic additions

| ID | Requirement | Acceptance evidence |
| --- | --- | --- |
| R22 | Verified original Honda ECU sensor/load interfaces | ECU identity, Federal variant, factory pin references, waveform/impedance/thermal captures; pending |
| R23 | Full engine emulation objective | Original ECU starting/running, applicable monitor and induced-fault records; synthetic core is not acceptance |
| R24 | Honda-mediated EV category indication on original OBD | Observed ECU code/MIL, prerequisites, delay, simultaneous categories and native-reader recovery; mappings unverified |
| R25 | Preserve Honda lamp and traction independence | Original ECU MIL ownership, independent failure indication, USB/Bluetooth detail and no diagnostic rearming; hardware validation pending |

## Whole-vehicle acceptance additions — 28 October 2026

| ID | Requirement | Acceptance evidence |
| --- | --- | --- |
| R26 | Wake County/NC administrative and equipment route | VIN/manufacture date, classification guidance, applicable inspection or accurately labeled voluntary equipment report; pending |
| R27 | Complete installed car at 75 mph for twenty minutes | Structural/HV/brake/chassis gates, agreed course duty and synchronized 25-mile target-speed records; pending |
| R28 | Thermal claims distinguish endurance from stabilization | Verified component limits, measured margins, final-window slope/span, ambient/airflow/payload/SOC and sensor-boundary limitations; pending |
| R29 | Complete auxiliary power, charging and retained body systems | LV load/backup tests, charging/drive interlock, visibility/lighting/steering/assist and no loss of essential functions on traction fault; pending |
| R30 | Funded, approved test logistics | Itemized remaining budget, owned/in-kind credits, independent review, venue/insurance/transport and agreed deadline; pending |

Hilly-route clarification: use 3%/6% uphill cases, with 6% over the full twenty
minutes as a conservative design stress screen until course data exists. This is
not a measured course profile or automatic professor acceptance. Include no-regen
descents and repeat the mass/energy calculation after actual battery packaging.

## Controller release additions — 29 October 2026

| ID | Requirement | Verification / status |
| --- | --- | --- |
| R31 | Component-specific thermal limits and data quality | Synthetic spike/filter/hysteresis/ID/stale cases; limits and hardware pending |
| R32 | Persistent, synchronized circulation detection | Synthetic uncertainty, skew, reversal and interruption cases; channel power measurement pending |
| R33 | Bounded dual-channel dispatch | Mock completion/age/skew/partial-send/cancellation tests; actual port and latency pending |
| R34 | Detailed fault provenance | Fixed component records, CRC detail telemetry and USB/Bluetooth mock parity; physical links pending |

See [current release](CONTROLLER_RELEASE.md). The new eight-file control scope has
separate source evidence; historical five-file formatting-only results remain archived.
