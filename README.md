# THE ASSET — Civic EV capstone

Code, tests and engineering work for a 1996 Civic DX EV conversion.

**Version one:** owned 2009 Prius inverter/transaxle, MG2 propulsion only.
The acceptance target is 75 mph for 20 minutes with hills; physical feasibility
and vehicle acceptance remain unverified. Independent MG1 propulsion is a later
upgrade. Simulator results are not hardware ratings or driving evidence.

## Repository map

| Path | Responsibility |
|---|---|
| [`src/control/`](src/control/) | Portable C++03 supervisor, thermal/fault monitoring and abstract dispatch |
| [`src/core/`](src/core/) | C11 reference simulator, mock CAN and battery/inverter/charger models |
| [`src/app/`](src/app/) / [`src/server/`](src/server/) | Host demos and loopback-only simulator UI |
| [`include/`](include/) | Public interfaces and shared types |
| [`firmware/`](firmware/) | Mega engine emulator, Teensy EV diagnostics and shared I/O library |
| [`firmware/parking_pawl/`](firmware/parking_pawl/) | Separate Uno parking-pawl review projects; outputs disabled |
| [`tools/`](tools/) | Offline analysis, reports, connector exports and review helpers |
| [`configs/`](configs/) | Study inputs, unverified hardware profiles and dependency locks |
| [`tests/`](tests/) | Host regressions and recorded-data analysis tests |
| [`docs/capstone/`](docs/capstone/) | Requirements, commissioning, vehicle studies and presentation guides |
| [`docs/parking-pawl/`](docs/parking-pawl/) | Pawl research, integration gaps and original compile evidence |
| [`legacy/`](legacy/) | Isolated automatic-transmission reference; not the manual-Civic baseline |

## Build and test

Requires CMake 3.16+, C11/C++ compilers and Python 3.12. Windows uses the
Visual Studio 2022 build tools for the commands below.

```powershell
python -m venv .venv-bench
.venv-bench/Scripts/python.exe -m pip install -r configs/bench-requirements.lock.txt
cmake -S . -B build -G "Visual Studio 17 2022" -DBUILD_TESTING=ON -DPython3_EXECUTABLE="$PWD/.venv-bench/Scripts/python.exe"
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

See [development commands](docs/DEVELOPMENT.md) for Unix, C++03 verification,
firmware builds and offline demo/report commands. Native simulator UI:
`build/Debug/bench_ev_harness.exe`, then <http://127.0.0.1:8080/>.

## Start reading

- [Current project plan](docs/capstone/TRELLO_REDESIGN.md) and [requirements](docs/capstone/REQUIREMENTS.md)
- [Code architecture and boundaries](docs/ARCHITECTURE.md)
- [Controller release and topology](docs/capstone/CONTROLLER_RELEASE.md)
- [Engine emulator installation](docs/capstone/ENGINE_SIMULATOR_INSTALL.md)
- [Parking-pawl integration review](docs/parking-pawl/INTEGRATION_REVIEW.md)
- [Offline bench analysis](docs/capstone/BENCH_TESTING.md)
- [Publication and validation record](docs/PUBLICATION.md)

## Evidence boundaries

The host simulator and analysis tools do not operate traction hardware. The
STM32F107 target work produces portable objects, not a flashable VCU port. Honda
profiles remain electrically disabled until verified; the original ECU owns OBD/MIL.
Parking-pawl builds disable actuator outputs. Archived sketches are outside those
release guarantees and are not deployment targets.

No measured continuous rating, inspection acceptance or full JSF compliance is
claimed. Historical 70 mph/Gen 3/dual-MG studies remain explicitly provisional for
the current Gen 2 MVP. The [JSF review](docs/capstone/JSF_REVIEW.md) includes pending
manual rules and requires a separately installed checker.
