# Code review guide for the examining board

> **29 October controller revision:** see [current controller and topology](CONTROLLER_RELEASE.md). The default Teensy now coordinates Honda-induced faults and USB details; Honda owns OBD/MIL. Earlier combined-gateway results describe the isolated legacy demonstration.


**Readability review, 28 October 2026.** This guide identifies responsibilities,
evidence and open questions. It is not a completed independent audit or a claim
that comments establish correctness.

## Review scope and conventions

The readability pass covers project-authored C/C++ under `include/`, `src/`,
`tests/` and the new `firmware/` package, plus Python tools and tests. Historical
Arduino parking-pawl/automatic-transmission sketches and external libraries remain
reference/dependency material. Embedded browser payload strings are preserved.
Those exclusions are not approved production implementations.

- C/C++ uses consistent four-space indentation, expanded statements and Allman
  braces. Python uses Black with a 100-column target. Long literal payloads and
  certain explanatory comments are retained rather than rewriting their contents.
- Line comments state purpose, units, clock domains, ownership, failure behavior
  and reasoning behind non-obvious arithmetic. They avoid merely narrating an
  assignment or treating an assumption as a measured rating.
- C++03 applies to the eight-file JSF supervisor/release scope. The separate
  firmware core uses C++11; the host reference uses C11. Python analysis retains
  its isolated environment. Firmware and dependencies have no blanket JSF claim.
- Hardware output gates, synthetic evidence labels and unresolved calibration
  requirements remain explicit. Readability edits do not release a vehicle profile.

## Suggested examination route

| Question | Entry point | What to trace |
| --- | --- | --- |
| Who authorizes motor operation? | `src/control/control_runtime.cpp` | Startup inhibit â†’ valid snapshot â†’ policy/latches â†’ mapping â†’ both sends â†’ inhibit acknowledgment |
| How do the machines share limits? | `src/control/dual_supervisor.cpp` | Both feedback checks, gearset constraints, thermal factor and shared simulated pack budget |
| What does a wire command mean? | `src/control/control_codec.cpp` | Validated raw digits, bit positions, counter and STM32 CRC; no assumed Nm scaling |
| How are virtual engine edges scheduled? | `firmware/lib/CapstoneIO/src/engine.cpp` | 720-degree units, relative deadlines, wrap arithmetic and missed-edge disarm |
| What runs inside interrupts? | `firmware/src/mega_engine.cpp` | Position/VSS timers, protected pulse capture, atomic foreground transactions and global gate |
| How do faults become codes? | `firmware/lib/CapstoneIO/src/diagnostics.cpp` | Internal source/history policy; indication.cpp maps EV categories to reviewed emulator stimuli |
| Can a reader affect traction? | `firmware/src/teensy_gateway.cpp` | No reader CAN or MIL ownership; native Honda diagnostics cannot rearm traction |
| What survives a power interruption? | `firmware/lib/CapstoneIO/src/journal.cpp` | Alternate slots, CRC, commit ordering, byte-write tests and unproven flash power-loss behavior |
| Are results defensible? | `tools/benchkit/analysis.py` | Evidence classification, synchronized physical channels, independent paired runs and separate calibration bounds |
| Can another person reproduce the evidence? | `tests/`, build instructions and `evidence/` | Host regressions, actual board compilation, hashes and explicitly synthetic presentation exports |

The source comments explain numeric Honda-link states next to their branches and
journal phases beside their storage offsets. The template Honda initialization,
electrical layers and waveform/sensor models still need exact-ECU validation.

## Reproduce readability checks

```powershell
python -m venv .venv-style
.\.venv-style\Scripts\python.exe -m pip install -r configs/readability-requirements.txt
python tools/check_readability.py --clang-format "C:/Program Files/LLVM/bin/clang-format.exe" --black-python .venv-style/Scripts/python.exe --report docs/capstone/evidence/readability-checks.json
```

The C/C++ style file is `configs/capstone-cpp-format.yaml`; the reviewed formatter
is clang-format 22.1.8. Existing C/C++ token streams and Python ASTs are compared
with the pre-edit baseline, ignoring comments, formatting positions and escaped
preprocessor line continuations. This checks a readability-only change; future
functional edits need their own reviewed baseline rather than silently replacing it.
The lightweight lexical check does not prove arbitrary preprocessor equivalence.

Use [firmware build instructions](../../firmware/README.md) and
[C++ supervisor instructions](CONTROL_CPP.md) for compilation/regression checks.
The five-file JSF scan and source metrics are refreshed after source formatting;
manual rule dispositions remain open. Independent sign-off, measured execution
timing and physical acceptance remain separate work.

## Questions that remain open

- Exact Honda ECU identity, sensor patterns/curves, monitored loads and full monitor behavior.
- MCU interrupt latency and watchdog/gate response under actual maximum traffic.
- Original Honda and inexpensive-reader behavior on the real electrical interfaces.
- Hardware-calibrated torque/current mapping, fault latency, cooling and vehicle acceptance.
- Manual JSF rule review, toolchain/platform assumptions and dependency assessment.

These are engineering evidence gaps, not items resolved by adding comments.

## Results of this readability pass

- Readability checks pass: 42 C/C++ files and 20 existing Python files; the new
  checker is formatted separately. Non-comment tokens and Python ASTs are unchanged.
- Both firmware targets compile, and both Intel HEX images are byte-for-byte
  identical to their pre-cleanup images. Debug/source-map artifacts may differ.
- All seven project suites pass on the final host build (80.67 seconds).
- The scoped JSF scan reports zero detected violations across 40 automated checks;
  193 manual-review items remain. Metrics: 27 functions, maximum 45 physical lines,
  maximum conservative branch metric 18.
- The regenerated compiled-core example (generated locally; not published)
  has the same waveform/code results as the earlier synthetic demonstration.

Current source hashes, raw logs and the read-only check results are recorded in
`evidence/readability-review.json` and the associated `readability-*` records.

## Current controller release evidence â€” 29 October 2026

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
