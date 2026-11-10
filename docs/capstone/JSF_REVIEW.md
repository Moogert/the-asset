# JSF C++ review record

## Standard and scope

The user confirmed Joint Strike Fighter Air Vehicle C++ Coding Standards,
document 2RDU00001, revision C, December 2005. The public-release standard is the
authority; extracted checker descriptions are a navigation aid.

**Current status: automated source checks complete; compliance not established.**
The C++03 supervisor/runtime library is implemented and host-tested. Its eight-file
scope is recorded in `configs/control-source-scope.json`; C11 reference code, host
tests/tools and upstream FOC are separate dependencies. Actual hardware adapters
and energy management remain unimplemented. Professor scope acceptance and any
language/toolchain deviations remain open. No deviation is approved.

## Local checker evaluation

Tool: `jsfav 0.1.0`, located in the adjacent `smash/jsfav-checker` workspace.
The README and source were inspected. The catalog accounts for 233 rule IDs:
40 automated checks and 193 review rules. Without compilation commands, only
12 file/line checks run; the skipped checks enter the review queue.

### Current C++ implementation scan

| Field | Observed result |
| --- | --- |
| Profile / language | literal / C++03 |
| Scope | Three control headers and five C++ implementation files |
| Compilation database | Actual Clang/Ninja build commands |
| Partial | false |
| Automated checks executed | 40 |
| Detected violations / suppressions | 0 / 0 |
| Review items / coverage warnings | 193 / 0 |

Evidence: `evidence/jsf-cpp-review.json`, `.sarif`,
`jsf-cpp-manifest.json` and `jsf-cpp-compile-commands.json`. The manifest pins
source, scope, CMake, tool, catalog and compilation-database hashes. Reproduce:

```powershell
python tools/run_jsf_review.py --checker `
  '../../smash/jsfav-checker/build-msvc/Release/jsfav.exe' `
  --compdb build-control-clang
```

The helper returns nonzero for a partial C++ scan or reported violations. It does
not mark manual rules passed. The 233-row worksheet remains open for final rule
dispositions; automated-no-findings is evidence, not proof of complete rule coverage.

Clang AST metrics for the three implementation files find 27 function definitions,
at most 45 physical lines per function and a maximum branch metric of 18. The
metric counts control branches and short-circuit operators; it supports rules 1/3
review, without replacing logical-SLOC interpretation or manual confirmation.
See `evidence/control-metrics.json` and `tools/review_control_metrics.py`.

Important open platform items: `stdint.h` is a C99 header used by the C++03
implementation; numeric semantics and execution timing need target review. Byte,
integer and Real storage widths are checked at compile time. Exceptions, RTTI and
fast-math are disabled for the control library. These facts alone do not establish
JSF compliance.

### Historical partial header baseline

On 27 October 2026, the literal-profile baseline command was:

```powershell
& '../../smash/jsfav-checker/build-msvc/Release/jsfav.exe' scan `
  include/dual_drive.h include/prius_inverter_adapter.h `
  --format json --output build-capstone/jsf-header-baseline.json
```

| Field | Observed result |
| --- | --- |
| Profile | literal |
| Scope | Only the two named C interface headers |
| Compilation database | Absent |
| Partial | true |
| Automated checks executed | 12 |
| Detected violations | 0 |
| Review items | 221 |
| Coverage warnings | 1 |
| Suppressed findings | 0 |

The checker selects C++ source/header suffixes; it does not select the `.c`
implementations. This result provides no semantic C++ coverage and no evidence
that the current control implementation satisfies all rules. Exit 0 means scan
completion. Do not rename C files merely to create the appearance of compliance.

The checker's own Windows Release CTest run passed both `cli` and
`local_web_server` suites in 123.10 seconds. This verifies its existing local
test suite, not completeness of its rule coverage. Linux was not rerun here.
Raw baseline results and file/tool hashes are retained under `evidence/`.
`evidence/jsf-rule-review.csv` contains all 233 rule IDs with every disposition
initially open. No row is silently marked compliant from the partial scan.

Reproduce the saved baseline and manifest from the repository root:

```powershell
python tools/run_jsf_review.py --checker `
  '../../smash/jsfav-checker/build-msvc/Release/jsfav.exe'
```

The script preserves an existing manual-review matrix. It does not mark rules
passed, modify control source, transmit messages or approve deviations.

## Remaining review and release evidence

1. Review the pinned standard, C++ baseline, compiler, checker hashes and source scope; accept or resolve the documented platform assumptions.
2. The host C++ target and compilation database exist. Add the actual MCU toolchain and separately assess dependencies; host syntax does not establish target timing or behavior.
3. The literal scan, JSON/SARIF and coverage evidence are saved. Re-run after source changes. `modern` is an alternate interpretation and requires an explicit course decision.
4. Create a disposition for every rule: verified with evidence, justified not applicable, approved deviation, or open. Check manual rules against the original text. Record reviewer/date and approval references only when they exist.
5. Review boundary assumptions, numeric ranges, initialization, deterministic timing, resource use, interfaces and test adequacy. Automated source-pattern checks cannot establish these by themselves.
6. Re-run analysis and tests after changes. Submit an honest open-item list; do not describe the checker as a certification tool.

The existing checker is suitable for finding supported patterns and organizing
review work. Its 40 automated checks are not equivalent to 40 fully proven rules
for all possible programs; validate relevant exceptions and findings against the
standard. Hardware commissioning and functional safety remain separate evidence.
