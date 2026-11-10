# Dual-motor Prius drivetrain for a Civic

> **29 October controller revision:** see [current controller and topology](CONTROLLER_RELEASE.md). The default Teensy now coordinates Honda-induced faults and USB details; Honda owns OBD/MIL. Earlier combined-gateway results describe the isolated legacy demonstration.


**First design and software review | 28 October 2026**

## 1. Objective and scope

Develop a coordinated MG1/MG2 drivetrain using the existing 2009 Prius transaxle for a 1996 Civic DX. Both machines must contribute measured output torque. The updated vehicle requirement is **75 mph for twenty minutes on a closed course**, with applicable NC safety/registration requirements and Wake County registration. Hills are required; exact course grades remain unknown, so the current study includes 3%/6% cases and a conservative full-duration 6% stress screen. Earlier 70 mph calculations below are retained as the historical baseline; use [the current vehicle-completion study](VEHICLE_COMPLETION.md) and [budget-extension memo](BUDGET_EXTENSION.md) for final acceptance planning.

**Current stage:** design analysis, tested C11 host simulation and a C++03 supervisor/runtime library. No traction hardware has been commissioned. Calculations estimate feasibility; they do not establish a continuous power rating or road readiness.

### Selected architecture

| Item | First-round baseline |
| --- | --- |
| Transaxle | Verify Gen 2/P112 identity; retain stock windings and resolvers |
| Mechanical change | Lock power-split planetary members together; retain normal differential operation |
| Inverter | Gen 3 Prius inverter with V1d dual-STM32 replacement board |
| Control | Independent OpenInverter FOC for each machine; external supervisor coordinates commands and permissions |
| Battery | Used Leaf pack under consideration; preserve 96-series architecture as the initial study case |
| Cooling | Shared inverter/transaxle coolant circuit, aluminum Civic radiator and standard fan |
| Pump | Listed BMW replacement 11517597715; exact interface still unverified |
| Tires | 205/45R15; loaded rolling circumference not measured |
| Vehicle brakes | Planned 986 Boxster base-front / 294 mm JCW plus Integra 4x100 rear discs; currently drums, no ABS; fitment/hydraulics pending |

V1d is the dual-motor board. Gen 2 single-channel and Gen 3 charging-oriented variants are not equivalent substitutes. The V1d internal logic-level CAN connection requires a reviewed physical-layer modification for the proposed external supervisor [3,4].

### Required demonstration

1. MG2-only and MG1-only operation under controlled load.
2. Combined operation produces more output torque than MG2 alone at matched speed and MG2 command, beyond measurement uncertainty.
3. Invalid feedback, controller loss or withdrawn battery permission inhibits both channels; recovery requires deliberate re-arming.
4. A documented speed, voltage and thermal envelope assesses the 70 mph target.

Use a guarded dynamometer or calibrated torque absorber. Unloaded rotation is an intermediate check. No combined rating is obtained by adding nameplate motor powers.

**Outside the initial scope:** winding redesign, OEM boost-converter control, mains charging, maximum-power experiments and complete vehicle installation. The mechanical lock must preserve alignment, bearing support, lubrication and debris control.

<!-- pagebreak -->

## 2. Physics, battery and cooling assessment

### Speed and electrical feasibility

For the Gen 2 gearset, `30 nMG1 + 78 nMG2 = 108 ncarrier` [1]. Locking all planetary members makes both motors turn together. Holding only the carrier stationary instead forces MG1 to turn oppositely at 2.6 times MG2 speed.

The selected tire has a geometric diameter of 0.5655 m and circumference of 1.7766 m. At 70 mph with 4.113:1 reduction, locked motor speed is approximately **4,347 rpm**. Holding the carrier would force approximately **11,302 rpm at MG1**. Actual loaded tire circumference and component speed limits require confirmation.

The model uses published 2004 Prius back-EMF data as a proxy for the unmeasured 2009 machines [1]. With MG2 at 0.09 line-line Vrms/rpm, 4,347 rpm gives about 391 Vrms. An ideal SVPWM screen with 90% voltage utilization gives `Vdc = sqrt(2) Eline,rms / 0.9`, approximately **615 V without field weakening**. This is a screening result, not a permissible supply voltage. A stock Leaf-voltage design therefore needs verified field weakening, available torque and thermal margin at highway speed. Measure back-EMF and resolve RMS/peak conventions before final selection.

### Road-load and Leaf-pack study

Assumptions: 1,300 kg converted mass, CdA 0.65 m2, rolling coefficient 0.012, air density 1.225 kg/m3, combined drivetrain efficiency 0.866 and 500 W auxiliary load. Wheel power is aerodynamic plus rolling plus grade force, multiplied by speed; battery power includes losses and auxiliaries.

| Continuous duty at 70 mph | Estimated battery power | Current at 300 V loaded |
| --- | --- | --- |
| Level | 20.1 kW | 67 A |
| 3% grade | 33.9 kW | 113 A |
| 6% grade | 47.7 kW | 159 A |

These are assumed duty cases, not measured demand or a demonstrated hill capability. An early 24 kWh Leaf example retains 48 original 2s2p modules as 96s2p: about 360 V nominal and 398.4 V full. Halving series count increases current and field-weakening demand. The preferred starting point is original series count; donor health, isolation, packaging, BMS, current limits and inverter voltage compatibility remain selection gates [7].

### Cooling, oil and orientation

Radiator size alone cannot establish heat rejection. The model brackets radiator conductance at 80/250/500 W/K and assumes 10 L/min installed coolant flow. These values, thermal resistances and efficiencies are engineering assumptions. A larger radiator does not remove winding-to-oil or oil-to-coolant resistance [2].

In the nominal level-road model, stock oil coupling predicts about 91 C bulk oil and 113 C hottest winding. An illustrative external oil-to-coolant exchanger can lower these estimates; targeting 80 C oil predicts about 108 C winding. **70-90 C oil is a provisional study band, not an OEM operating specification or approved controller setting.** Optimize using measured oil, winding, coolant and load data; bulk oil does not resolve magnet hotspots.

Retain the original transaxle attitude pending lubrication review. Rotating the entire assembly does not improve electromagnetic torque. Retain internal stator clocking and recalibrate resolver offsets if disturbed. Electrical field angle is optimized through FOC, current calibration, MTPA and field weakening, not by physically rotating a stator.

The pump catalog cross-reference identifies a CWA200 family, but available pinout references conflict. Confirm the exact unit diagram before wiring, then perform a flooded bench test and an installed-flow test. No-signal full-speed behavior is unproven for this unit. BSD, LIN and PWM are distinct interfaces [8].

<!-- pagebreak -->

### Battery enclosure and selection decision

Prefer **retaining the rear seats**, using the removed fuel-tank cavity first and a split protected enclosure if needed. Feasibility depends on measured space, module restraint, ground/suspension clearance, structural mounts and axle limits. Relocate intact Leaf modules rather than opening pouch assemblies. Leaf is provisional because it reduces cell-joining and validation work; new cylindrical cells may offer better packaging/mass. The [battery comparison and enclosure study](BATTERY_PACKAGING.md) compares equal usable energy, installed-mass limitations and cost: a 24 kWh Leaf donor example allows **$4,563–$12,000**, versus **$12,750–$24,625** for a new 96s25p/24.19 kWh cylindrical example, both including 25% contingency but excluding labor, charger and major structural work. These are engineering allowances, not quotes. Donor health and actual mission energy determine the final choice.

## 3. Control software and first-review evidence

### Responsibility split

`Pedal / brake / BMS -> supervisor -> independent MG1 and MG2 FOC -> locked gearset -> differential`

OpenInverter runs rotor-position processing and fast current control independently for each motor. The new supervisor selects mode, allocates shared battery limits, checks health and authorizes drive. It must not treat throttle digits or normalized effort as calibrated newton-metres.

| Implemented and host-tested | Remaining for hardware operation |
| --- | --- |
| C++ MG2-only, MG1-only and combined policy; C11 reference | MCU integration and scheduled execution |
| Separate feedback and shared discharge/regen allocation | Verified controller IDs, telemetry and BMS decoding |
| Overspeed, temperature and shaft-speed plausibility checks | Measured motor limits, phase/resolver calibration |
| Stale-data, latched inhibition and mocked watchdog/inhibit | Physical circuitry and timing capture |
| CRC/sequence checks for synthetic feedback | Real CAN transceivers, wiring and captures |
| C++ v5.40.R encoder and abstract two-channel dispatch | Calibrated mapping, actual driver and validated telemetry |

All host scenario IDs are synthetic. The offline encoder implements a pinned upstream message format but transmits nothing. The traction host harness has no physical CAN, gate, contactor or charger outputs; the separate Arduino diagnostic subsystem is described below. Simulation allocation is a linear DC-current envelope, not a motor torque map or closed-loop battery-current regulator.

### Verification completed

The Windows MSVC Debug build passes seven CTest suites: Civic firmware core, harness behavior, dual-drive policy, C++ control regression, physics models, loopback HTTP integration and offline bench analysis. The C++ tests compare 6,494 cases with the C11 reference. Existing tests include 2,091 allocation combinations and ten Python model checks. The new bench package imports recorded CSVs, analyzes tuning/torque/fault/thermal experiments and exports presentation figures; its supplied demonstrations are synthetic. A separate Clang build verifies C++03 syntax and passes the C++ regression suite.

These tests establish software behavior under simulated inputs. They do not prove measured torque, thermal capacity, physical inhibit latency, or a working V1d CAN modification. The native upstream timeout is too long to establish the proposed 100 ms physical shutdown target by itself [5].

### JSF C++ extra-credit approach

Use the Joint Strike Fighter Air Vehicle C++ Coding Standards, document 2RDU00001, revision C, December 2005 [9]. **Compliance is not yet established.** The proposed scope now contains five C++ supervisor/runtime files; host UI, tests, Python models and upstream FOC are separately identified. No physical hardware adapter is implemented.

The local `jsfav` checker accounts for 233 rules. The current literal-profile scan uses the actual C++ compilation database: all 40 automated checks ran, with zero reported violations, zero coverage warnings, no suppressions and 193 manual review items. JSON/SARIF, source/tool hashes and a 233-rule worksheet are saved. The older partial header scan is retained as history.

Next complete manual rule dispositions and platform/toolchain review, including fixed-width integer headers and measured execution timing. The C++ port and compiler checks support that review; they do not replace it. A zero scanner exit code means scan completion, not compliance. No deviation is approved.

<!-- pagebreak -->

### Original Honda electronics and EV diagnostics

A separate [Arduino electronics work package](HONDA_ECU_AND_OBD.md) now contains
Mega 2560 engine-signal firmware and a Teensy 4.1 combined Honda/EV OBD gateway.
It includes timer-driven synthetic scenarios, fault history, a code dictionary,
protected interface schedule and software tests. Non-California/Federal equipment
is user reported; exact ECU identity and all Honda cavities remain unverified.
Full engine-monitor operation and inexpensive-reader compatibility require physical
acceptance. This subsystem is outside the five-file JSF supervisor review scope.
Its parts allowance is **$778–$1,782 including 25% contingency**, excluding labor,
instruments, ECU purchase if required and shipping/tax.

## 4. Next work, acceptance gates and resources

### Control-software sequence

| Gate | Deliverable and acceptance |
| --- | --- |
| A. Review baseline | Freeze requirements, state model, interfaces and tests; agree JSF scope and acceptance method with professor |
| B. C++ supervisor | Port and host tests complete; finish manual JSF/platform review and target-specific timing/resource evidence |
| C. Logic-only hardware | Confirm exact board schematic; implement CAN interfaces, distinct nodes, telemetry, watchdog and independent inhibit; capture failure timing without traction power |
| D. Individual machines | Limited-energy MG2 commissioning, then MG1; verify current signs, resolver offsets, phase order and current loops |
| E. Loaded combined drive | Calibrate current/torque maps; prove added torque beyond uncertainty and common fault behavior; characterize regen and thermal envelope |

Nominal supervisor command period is 10 ms. Measure the complete fault-to-inhibit path; do not equate a software stale threshold with physical shutdown latency. Precharge, contactor feedback, discharge and fault recovery require an explicit hardware energy-management state machine. No automatic restart into torque is permitted.

### Project schedule and limits

Provisional 16 weeks after hardware and lab access: weeks 1-2 identify/review; 3-5 interfaces and logic-only tests; 6-8 mechanical work and individual motors; 9-11 loaded combination; 12-14 speed/thermal/regen tests; 15-16 repeated acceptance and final archive. Long-lead parts and failed gates may change this schedule.

Allow $3,000-$6,000 beyond the existing transaxle, plus 25% contingency after quotations. This planning allowance excludes the traction battery, major Civic installation, purchased lab instruments and dyno rental. It is not a supplier quotation.

The [complete brake BOM](BRAKES.md) now includes Porsche fronts, Integra 4x100 rear discs, parking cables and electric vacuum assist: **$3,334–$5,769 for parts with 25% contingency**, or **$4,834–$8,644 including labor and contingency**. Master replacement, conditional hard-line/wheel work, tax/shipping and approval costs remain extra. Master bore, proportioning and balance require review; a larger master or wholesale hard-line replacement is not assumed.

Immediate unresolved inputs: verified transaxle/inverter/board identities; loaded tire circumference; Leaf donor voltage/health/current limits; hill grade/duration; radiator dimensions/shroud; exact pump identity and installed flow. Mechanical lubrication, highway field weakening and continuous heat rejection are the main feasibility risks.

Final evidence will include as-built drawings, source/configuration hashes, separate motor calibrations, CAN captures, fault timing, measured torque-speed curves, thermal limits and an uncertainty-aware acceptance report. Vehicle mounts, axles, brakes, battery enclosure and parking controls follow drivetrain acceptance.

### References and supporting files

1. [ORNL Prius evaluation: gearing and back-EMF](https://info.ornl.gov/sites/publications/files/Pub1870.pdf).
2. [ORNL Prius thermal evaluation](https://info.ornl.gov/sites/publications/Files/Pub57487.pdf).
3. [Gen 3 board documentation](https://openinverter.org/wiki/Toyota_Prius_Gen3_Board) and [hardware source](https://github.com/damienmaguire/Prius-Gen3-Inverter).
4. [Developer explanation of internal CAN](https://openinverter.org/forum/viewtopic.php?p=35828#p35828).
5. [Pinned FOC v5.40.R source](https://github.com/jsphuebner/stm32-sine/tree/v5.40.R) and [FOC commissioning guide](https://openinverter.org/wiki/Using_FOC_Software).
6. [ZombieVerter supervisor source](https://github.com/damienmaguire/Stm32-vcu). Existing adapter is reference code, not a verified dual-motor implementation.
7. [Nissan Leaf first-responder guide](https://owners.nissanusa.com/content/techpub/ManualsAndGuides/LEAF/2016/2016_LEAF-first-responders-guide.pdf). Confirm actual donor; module construction varies.
8. [Pump selection notes](PUMP_AND_LEAF_SELECTION.md) and [exact-unit test procedure](PUMP_TEST.md), including manufacturer and interface references.
9. [Public-release JSF C++ standard](https://stroustrup.com/JSF-AV-rules.pdf).

Repository entry point: `docs/capstone/README.md`. `BRAKES.md` adds the vehicle brake conversion, BOM and hydraulics decisions. `BENCH_TESTING.md` documents the offline measurement/report workflow; `BENCH_LAB.md` defines the fixture concept and lab gates. `CONTROL_CPP.md` provides build/demo instructions; `CONTROL_REVIEW.md` defines next deliverables; `JSF_REVIEW.md` records checker scope and limitations.
