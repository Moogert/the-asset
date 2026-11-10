# Prius dual-motor capstone implementation

[Current Trello plan](TRELLO_REDESIGN.md): MG2-only Gen 2 MVP, later dual-MG upgrade,
filterable priorities, readable cards and [tools/lab schedule](TOOLS_AND_LAB.md).

[Engine simulator installation and connector guide](ENGINE_SIMULATOR_INSTALL.md):
removable Civic ECU interposer, X1–X5 cavity schedule, Mermaid diagrams and firmware setup.

> **29 October controller revision:** see [current controller and topology](CONTROLLER_RELEASE.md). The default Teensy now coordinates Honda-induced faults and USB details; Honda owns OBD/MIL. Earlier combined-gateway results describe the isolated legacy demonstration.


## Current whole-vehicle target

The user now requires **75 mph for twenty minutes on a closed course**, with
hilly-course capability, applicable NC safety requirements and Wake County registration. Read the
[vehicle completion/inspection plan](VEHICLE_COMPLETION.md) and
[budget-extension memo](BUDGET_EXTENSION.md). Earlier 70 mph results remain
historical studies; the new 75 mph scenario does not establish physical capability.


[Examining-board code review guide](CODE_REVIEW.md): source responsibilities,
comment/style conventions, reproducible readability checks and remaining evidence gaps.


## Honda electronics and unified fault codes

The [Honda ECU and OBD work package](HONDA_ECU_AND_OBD.md) adds Mega 2560
engine-signal firmware and a Teensy 4.1 diagnostic gateway, a costed BOM and a
[protected interface schedule](HONDA_HARNESS.md). Board builds and host tests
are separate from the pending physical ECU/harness/reader acceptance.


The [battery packaging study](BATTERY_PACKAGING.md) compares Leaf modules
with custom cell packs, prioritizes the removed fuel-tank space and retained rear
seats, and includes an enclosure specification and cost allowances. The
[brake BOM](BRAKES.md) now includes the planned Integra rear-disc swap.


## Vehicle brake conversion

The [Porsche brake work package](BRAKES.md) incorporates the earlier Brembo
planning into this project, with a costed BOM, electric vacuum-assist requirements,
master-cylinder/hard-line decisions and road-release tests. The user confirms
stock-size DX front brakes, rear drums and no ABS. Kit selection and the planned Integra rear-disc hydraulic
balance remain pending physical identification/review; no installation is claimed.

## Closed-loop bench package

The [offline bench workflow](BENCH_TESTING.md) imports recorded measurements,
analyzes tuning/torque/fault/thermal tests and exports Markdown, SVG, 300 dpi PNG,
CSV and JSON. Read [the fixture and lab procedure](BENCH_LAB.md) before physical
testing. The fixture is concept-only; no hardware drivers or energized operation
are supplied. The user reports the full drivetrain is available; exact revisions
and instrument models are still unconfirmed.

## First-round submission

Start with [the professor-facing report](FIRST_REVIEW.md). It summarizes the
objective, selected hardware, physics estimates, cooling/oil assessment, software
evidence and remaining acceptance gates in four short sections. Its Markdown is
the submission source and can be exported directly.

For the controls review, include [control software and next implementation](CONTROL_REVIEW.md)
and [JSF C++ review record](JSF_REVIEW.md). A C++03 supervisor/runtime library now
passes comparison and mocked-I/O tests; see [implementation and build instructions](CONTROL_CPP.md).
The checker reports zero violations across 40 automated checks with the actual
compilation database. The 193 manual review items remain open; compliance is not claimed.

| Supporting document | Purpose |
| --- | --- |
| [Requirements](REQUIREMENTS.md) | Requirement-to-evidence status |
| [Validation](VALIDATION.md) | Reproduction commands and actual test limits |
| [CAN interface](CAN_INTERFACE.md) | Proposed hardware integration and message boundaries |
| [Commissioning](COMMISSIONING.md) | Sequential motor and fault acceptance procedure |
| [Engineering](ENGINEERING.md) | Mechanical scope, purchasing and 16-week schedule |
| [Physics estimates](PHYSICS_ESTIMATES.md) | Model equations and back-EMF assumptions |
| [Oil and orientation](OIL_AND_ORIENTATION.md) | Thermal network and three orientation meanings |
| [Pump and Leaf selection](PUMP_AND_LEAF_SELECTION.md) | Battery comparison and unresolved pump-interface evidence |
| [Pump test](PUMP_TEST.md) | Exact-unit identification, wet bench and installed-flow checks |
| [Porsche brake conversion](BRAKES.md) | Front conversion BOM/cost, rear-drum compatibility, hydraulics, EV assist and validation |

Submission figures use the current 205/45R15 configuration. Older worked tire
examples in supporting notes are illustrative, not the selected vehicle input.
The professor-facing report is the concise current summary; detailed notes retain
calculations and procedures needed to reproduce or challenge its assumptions.

Target: a verified Gen 2 P112 transaxle, locked power-split planetary assembly,
Gen 3 inverter with V1d dual-motor board, external supervisor, and a 1996 Civic DX.
Required capstone result: measured additional torque with both machines active.
70 mph is a design target pending voltage, loaded power, thermal and mechanical tests.

## What is implemented

- Portable C supervisor policy with MG2-only, MG1-only, combined and regeneration
  operation; independent feedback; shared simulated DC-current allocations;
  thermal derating; overspeed, feedback, scheduler and circulation checks; latched inhibit.
- Separate C++03 policy/runtime library with 6,494 C-reference comparisons,
  command-rate limiting, BMS freshness, startup interlock, abstract transport,
  independent counters and mocked watchdog/inhibit tests. The UI still uses the C harness.
- Existing loopback harness extended with CRC/sequence-checked MG1/MG2 simulated
  messages, per-channel fault injection and visible dual-channel state.
- Offline v5.40.R control-message encoder with STM32 CRC semantics. No transmitter.
- Reproducible source provenance and separate non-flashable commissioning worksheets.
- Parameterized drivetrain/road-load/voltage calculations and a measured-run
  comparison tool. No fabricated measurements or continuous-power claims.
- Regression tests, commissioning procedure, CAN integration specification,
  requirements/verification matrix, work schedule and purchasing worksheet.

## Run

```powershell
cmake -S . -B build-capstone -G "Visual Studio 17 2022" -DBUILD_TESTING=ON
cmake --build build-capstone --config Debug
ctest --test-dir build-capstone -C Debug --output-on-failure
.\build-capstone\Debug\bench_ev_harness.exe
```

For the new C++ runtime demonstration, run
`.\build-capstone\Debug\dual_control_demo.exe`. It prints combined-drive,
fault, latched-recovery and reset behavior with mock ports only. See
[CONTROL_CPP.md](CONTROL_CPP.md) for the separate Clang/JSF build.

Open <http://127.0.0.1:8080/>. Scenario buttons reset the simulator and select the
test mode. Both feedback channels remain mandatory in every drive mode because
both machines are mechanically connected. Fault removal does not re-enable drive;
the simulation Reset button starts an entirely new simulated session.

Using any working Python 3 interpreter:

```powershell
python tools/drivetrain_analysis.py --output build-capstone/vehicle-analysis.json
python tools/thermal_analysis.py --output build-capstone/thermal-analysis.json
python -m unittest discover -s tests -p test_analysis.py -v
python tools/analyze_dyno.py docs/capstone/dyno-template.csv
```

The empty dyno template intentionally fails until measured steady-state samples
are supplied. The updated vehicle configuration uses the user-selected 205/45R15
geometric diameter (0.5655 m), producing about 4,347 motor rpm at 70 mph with
4.113 reduction. Actual loaded rolling circumference is still unknown. Its unresolved
electrical and thermal gates remain unresolved, not assumed to pass.

## Hardware boundary and unfinished work

There are **no real CAN, gate-drive, contactor or charger outputs**. This code is
not an installed ZombieVerter firmware port. Actual board revision, pin-level CAN
modification, isolated inhibit circuitry, BMS adapter, real telemetry mapping,
calibration and loaded validation require the physical hardware and lab.
This software work has not ordered parts, flashed boards, modified gears or run
motors. Physical procurement/assembly history is user-supplied and uninspected.

All values returned by `dual_simulation_config()` are invented simulation
envelopes: 5,000 rpm, 70/90 C derating/stop, 100/150 A full-command DC draw and
40/60 A regeneration. They are **not motor or inverter ratings**. Linear predicted
current allocation cannot substitute for real measured pack-current control.

Read [engineering and purchasing](ENGINEERING.md), [CAN interface](CAN_INTERFACE.md),
[commissioning](COMMISSIONING.md), [physics estimates](PHYSICS_ESTIMATES.md) and
[requirements](REQUIREMENTS.md) before hardware work.
The next refinement is documented in [oil and orientation](OIL_AND_ORIENTATION.md):
build-input worksheet, grade/voltage sweeps and a separate bulk-oil thermal network.
See [pump and Leaf selection](PUMP_AND_LEAF_SELECTION.md) for the confirmed build
inputs, CWA200 part-family cross-reference and early Leaf architecture comparison.
Use the [pump test guide](PUMP_TEST.md) to confirm wiring and actual pump behavior.
Source commits are locked in `configs/upstream-lock.json`; motor parameters remain
null in the two commissioning files until measured. These JSON files are worksheets,
not OpenInverter parameter exports.
