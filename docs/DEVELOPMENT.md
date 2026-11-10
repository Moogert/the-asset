# Development commands

Run from the repository root. Generated files stay in ignored build/output folders.
No command below flashes a controller or operates physical hardware.

On Windows, prefer a short checkout/build path. Deep OneDrive paths can exceed
MSBuild's 260-character limit; use a short out-of-tree build directory if needed.

## Host tests

Install Python 3.12, CMake and a C11/C++ compiler. Create `.venv-bench` and install
`configs/bench-requirements.lock.txt`. Pass that interpreter to CMake so the bench
suite is included rather than silently skipped.

Windows/Visual Studio 2022 commands are in the root README. On Unix:

```sh
python3.12 -m venv .venv-bench
.venv-bench/bin/python -m pip install -r configs/bench-requirements.lock.txt
cmake -S . -B build -DBUILD_TESTING=ON -DPython3_EXECUTABLE="$PWD/.venv-bench/bin/python"
cmake --build build
ctest --test-dir build --output-on-failure
```

For Clang, set `CMAKE_C_COMPILER` and `CMAKE_CXX_COMPILER` in a separate build
directory. The control library uses the actual C++03 parser with exceptions/RTTI
disabled; MSVC alone cannot establish C++03 syntax compatibility.

A compiler-only syntax check needs no linker/startup code. For example, on Unix:

```sh
clang++ -std=c++03 -fno-exceptions -fno-rtti -fno-fast-math -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror -I include -fsyntax-only src/control/*.cpp
```

## Firmware review builds

```powershell
python -m venv .venv-firmware
.venv-firmware/Scripts/python.exe -m pip install -r configs/firmware-requirements.lock.txt
.venv-firmware/Scripts/python.exe -m platformio run -d firmware
.venv-firmware/Scripts/python.exe -m platformio run -d firmware/parking_pawl/PriusParkPawlBenchController
.venv-firmware/Scripts/python.exe -m platformio run -d firmware/parking_pawl/PriusParkPawlVehicleController
```

Use `.venv-firmware/bin/python` on Unix. Pinned platforms/packages live in each
`platformio.ini`. The diagnostic legacy OBD target is opt-in and isolated.

## Offline examples

```powershell
.venv-bench/Scripts/python.exe tools/bench.py plan --out output/bench/plan01
.venv-bench/Scripts/python.exe tools/bench.py demo --out output/bench/demo01
.venv-bench/Scripts/python.exe tools/drivetrain_analysis.py --config configs/vehicle-75mph-study.json --output output/vehicle-study.json
.venv-bench/Scripts/python.exe tools/export_emulator_harness.py --check
```

Create new output folders for repeat runs. Demo data are explicitly synthetic.
Read the bench guide before importing real measurements or interpreting metrics.

## Optional review tools

The formatting versions are in `configs/readability-requirements.txt`. JSF checking
requires a separate `jsfav` installation supplied through `--checker`; its source
and executable are not bundled here. Historical evidence records are retained,
with local machine paths redacted. Source changes require fresh review rather
than a claim that the old formatting-only baseline proves equivalence.
