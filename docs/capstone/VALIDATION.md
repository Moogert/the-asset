# Local verification record

Last rerun: **28 October 2026**. All seven project suites pass on the Windows/MSVC
Debug build. A separate Clang C++03 build passes the C++ control regression suite.
These are local software tests, not hardware
acceptance. The first-review summary is [FIRST_REVIEW.md](FIRST_REVIEW.md).

Full six-suite bench-package run passed in 83.83 seconds; its raw log is
`evidence/bench-validation.txt`. The final bench-analysis rerun passed in 56.50
seconds and is recorded in `evidence/bench-analysis-validation.txt` (35 tests).
It includes import integrity, analytic responses, independent paired-trial
statistics, missing/noisy/undersampled data, classification and presentation export.

Earlier saved runs: MSVC five suites passed in 8.68 seconds; Clang C++ regression
passed in 5.86 seconds. Raw logs are `evidence/control-validation-msvc.txt` and
`evidence/control-validation-clang.txt`. The console demo also ran successfully;
`evidence/control-demo.txt` shows inhibition persists after feedback returns until
a deliberate disarmed reset. No physical CAN device is accessed by that demo.

MSVC Debug build succeeded using Visual Studio 2022 Build Tools (MSVC 19.44),
Windows SDK 10.0.26100.0, CMake and the bundled Python 3 runtime.

Command: `ctest --test-dir build-capstone -C Debug --output-on-failure`.

| Suite | Result | Coverage |
| --- | --- | --- |
| harness_regression | Passed | Existing safe-start, drive, BMS, pedal, CAN, charger, HVIL, emergency stop and frame handling |
| dual_drive_regression | Passed | Modes; 2,091 request/budget combinations; regen rejection; per-channel faults; exact 100 ms stale boundary; no automatic restart; thermal derating; unequal shaft speeds; offline CRC/encoding; integrated fault injections |
| cpp_control_regression | Passed (MSVC and Clang) | 6,494 C/C++ policy comparisons; 260 valid codec comparisons; invalid codec fields; mocked dispatch, slew, startup, BMS freshness, clock wrap, timing budgets, driver failures and latched recovery |
| drivetrain_analysis_regression | Passed (10 Python tests) | Kinematics, road load, invalid inputs, back-EMF, thermal/oil balances, exchanger bounds, 80 C control screening, tire precedence, 205/45R15 geometry, Leaf series/parallel energy/current comparisons and measured-run validation |
| loopback_http_regression | Passed | Child server launch; UI assets; combined drive; single-channel loss shuts both; latched recovery; regen; fragmented header/body request |
| bench_analysis_regression | Passed (35 Python tests) | Planning/import, units/time/provenance, analytic responses, paired-run uncertainty, signed power, physical timing eligibility, thermal stability and seven SVG/300 dpi PNG figure sets |

The HTTP test launches its own loopback process on an available port and terminates
only that child. The server supports `--port` for independent tests. Fragmented
request handling was fixed after a smoke test exposed premature nonblocking reads.

`tools/drivetrain_analysis.py` and `tools/thermal_analysis.py` were run against the
saved default configurations. Their JSON outputs live in `build-capstone/` and
explicitly identify assumptions and unresolved hardware gates.
`oil_cooling_analysis.py` and `refine_build.py` were also run. The new oil-network
and build-refinement JSON outputs remain provisional estimates; the build input
worksheet records unknown details as null rather than inventing measurements.

No hardware was flashed, energized, physically modified or dyno tested. The
offline CAN encoder has not been verified with a scope/capture on actual STM32
hardware. No real loaded torque improvement, thermal rating, CAN modification or
100 ms physical inhibit latency is claimed by these passing software tests.

## Offline bench deliverables

The isolated Python 3.12 environment uses the exact versions in
`configs/bench-requirements.lock.txt`. The fixture plan and synthetic campaign
were generated through `tools/bench.py`; seven figure sets were inspected for
readable units, labels, bounds and non-measured classification. Charge current
and its acceptance boundary use consistent signed plotting. No demonstration
calibration is a physical certificate and no demo gain is a firmware recommendation.

See [BENCH_TESTING.md](BENCH_TESTING.md) for reproduction and
[BENCH_LAB.md](BENCH_LAB.md) for the unapproved fixture/instrumentation concept.
Actual MCU/DAQ drivers and loaded hardware tests remain pending.

## Vehicle brake documentation

The Porsche conversion is integrated in [BRAKES.md](BRAKES.md), the professor
report and requirements R16-R18. The prior user guide is archived byte-for-byte.
`evidence/brake-package-review.json` records the source hashes and checked budget
arithmetic: $1,672-$2,755 front/assist parts before contingency; $2,090-$3,444
parts with contingency; $3,090-$5,319 including the labor allowance and contingency.
That earlier front-only snapshot excluded rear/master/wheel work and tax/shipping.
The 28 October update below supersedes its budget scope. JSON validity,
local documentation links and all ten existing physics/model tests were checked.
These checks do not establish hydraulic compatibility, road approval or measured
braking performance for the confirmed drum/non-ABS vehicle.

## JSF checker evaluation

The current five-file C++ scope is analyzed using actual Clang/Ninja build
commands. Result: `partial: false`, C++03, 40 automated checks, zero reported
violations, zero coverage warnings, zero suppressions and 193 manual review items.
JSON/SARIF and the source/tool/build hashes are retained in `evidence/`.
The conservative AST review metric reports 27 implementation functions, maximum
45 physical lines and maximum branch metric 18. These are review aids, not a full
JSF determination. No deviation is approved; manual dispositions remain open.

The adjacent `jsfav-checker` Windows Release build reports version 0.1.0. Its
existing `cli` and `local_web_server` CTest suites were rerun and both passed
(123.10 seconds total). Linux was not tested in this session.

The historical literal-profile scan of `include/dual_drive.h` and
`include/prius_inverter_adapter.h` completed with `partial: true`: 12 automated
checks, zero detected violations, 221 review items and one coverage warning.
That initial scan had no compilation database or C++ implementation coverage. Raw results,
source/tool hashes and the open rule matrix are in `evidence/`. See
[JSF_REVIEW.md](JSF_REVIEW.md); this result does not establish compliance.

## Battery packaging and Integra rear BOM — 28 October 2026

Documentation/configuration update only; the earlier control and bench test dates
above are unchanged. The battery study records rear-seat retention and fuel-tank
cavity assessment, equal-energy comparisons, enclosure requirements and cost
allowances. The rear-disc BOM replaces the generic rear-conversion contingency.
Budget arithmetic, example energy/current/mass calculations, JSON syntax and local
links were checked. All ten existing physics/model regressions pass again.
Evidence: `evidence/battery-and-rear-brake-review.json` and the refreshed
`evidence/brake-package-review.json`. No packaging dimensions, donor health,
structural performance, hydraulic suitability or supplier quotes were established.

## Honda emulator and unified diagnostics — 28 October 2026

Both PlatformIO targets compile: Mega ATmega2560 and Teensy 4.1. The supplied
vehicle profile remains unverified; compilation is not ECU/reader acceptance.
The new portable C++11 core is outside the JSF scope. Its 5,715 host checks pass,
including artificial pulse count/phase, time wrap, malformed commands, missed-edge
disarm, actuator-response teaching model, fault mapping, OBD payloads, ISO-TP,
telemetry counter/CRC handling and interrupted NV-journal writes.

All seven CTest suites pass in 57.15 seconds. Raw records are
`evidence/civic-project-validation.txt`, `evidence/civic-core-validation.txt` and
`evidence/civic-firmware-build.txt`. Framework-library warnings are retained;
no warning from the project sources remains in the final incremental board build.
A refreshed scan of the unchanged five-file JSF supervisor scope still reports
40 automated checks, zero violations and 193 manual review items.

The compiled-core synthetic demonstration exports JSON, CSV, SVG and a 300 dpi
PNG under `output/civic-review/`; its plot was visually inspected for readable
labels and explicit synthetic classification. The ANCEL AD310 is a purchasing/test
candidate, not a physically tested reader. No board was flashed, ECU connected,
harness patched, lamp driven or monitor/readiness result established.

## Readability and examiner handoff — 28 October 2026

The scoped readability pass formats 42 project-authored C/C++ files and 20
existing Python files, plus the new non-mutating review checker. Curated line
comments explain contracts, units, timing, ISR ownership, failure paths and
synthetic/hardware boundaries. C/C++ non-comment tokens and Python ASTs remain
identical to the saved pre-edit baseline; this is a review aid, not a formal
equivalence proof. Formatting commands and the examination route are in
[CODE_REVIEW.md](CODE_REVIEW.md).

Both MCU targets compile again. The refreshed five-file JSF scan reports zero
violations, zero suppressions/coverage warnings, 40 automated checks and 193 manual
review items. Current source metrics are 27 functions, maximum 45 physical lines
and branch metric 18. Hardware acceptance and full JSF compliance remain open.
Raw checks/build/test evidence uses the `evidence/readability-*` filenames.

Final readability rerun: all seven suites pass in **80.67 seconds**; both
board HEX images match the previous delivery byte-for-byte. The regenerated
`output/civic-review-readability/` data matches the prior synthetic core results.

## Controller release verification — 29 October 2026

The eight-file C++03 scope now includes thermal/circulation monitoring and the
bounded dispatch release layer. The original Honda ECU owns OBD and MIL; default
Teensy firmware coordinates emulator fault categories and detailed EV records.
Earlier five-file and combined-gateway entries above are historical results.

- MSVC: all eight CTest suites passed, including 147 new revision checks.
- Clang: both control suites passed; portable control syntax is C++03.
- STM32F107: five control implementation objects cross-compiled for Cortex-M3,
  software floating point, warnings treated as errors. No linked MCU image/port.
- Mega2560 and Teensy4.1 default images built; the isolated legacy OBD image also built.
- JSF: 40 automated rules, zero violations/suppressions/coverage warnings;
  193 manual rules remain. No full-compliance claim.
- Source metrics: 55 functions, maximum 80 physical lines, conservative branch
  metric maximum 18. Current format check passed for 52 C/C++ files and 22 Python
  entries; no equivalence to the earlier formatting-only baseline is claimed.
- Four synthetic replays export CSV/JSON/Markdown/SVG/300 dpi PNG. Labels were
  visually inspected. No ECU, instrument, Bluetooth radio or traction hardware was used.

Raw logs and pinned sources: `evidence/controller-release-review.json` and
`evidence/controller-release-*.txt`. The current guide is
[CONTROLLER_RELEASE.md](CONTROLLER_RELEASE.md). Third-party framework warnings
remain visible in build logs; no project-source warning was reported by the final
control/firmware builds. Target stack use, electrical timing, actual Honda codes,
reader compatibility, full engine-monitor emulation and production limits remain pending.
