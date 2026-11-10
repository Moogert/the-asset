# C++ supervisor implementation

> **29 October controller revision:** see [current controller and topology](CONTROLLER_RELEASE.md). The default Teensy now coordinates Honda-induced faults and USB details; Honda owns OBD/MIL. Earlier combined-gateway results describe the isolated legacy demonstration.


27 October 2026. Review-stage library; no MCU image or physical output driver.

## What is built

| Module | Responsibility |
| --- | --- |
| `dual_supervisor.cpp` | Mode selection, shared current allocation, feedback checks, thermal factors and latched policy faults |
| `control_codec.cpp` | Offline v5.40.R control payload, two-bit counter and STM32 CRC |
| `control_runtime.cpp` | Periodic execution, BMS freshness, startup interlock, slew limiting, dispatch and watchdog/inhibit requests |
| `Hardwareport` | Abstract boundary for calibrated mapping, CAN queueing, inhibit and watchdog; no hardware implementation supplied |

The C11 harness remains the comparison reference and UI simulator. It has not
been silently switched to the new runtime. Tests exercise the C++ library through
mocked ports; both versions remain available for review.

## Verified behavior

- 6,494 direct C/C++ comparisons cover all three modes, allocation sweeps, motor faults, stale timing, thermal allocation, scheduler checks and fault recovery.
- 260 valid codec vectors match the C encoder byte for byte; malformed ranges/counters are rejected.
- Runtime tests cover boot while armed, disarmed startup, independent IDs/counters, first/second transmit failure, mapping failure, inhibit-driver failure, stale/future BMS data, future motor timestamps, energy permission, clock wrap, timing budgets and latched recovery.
- Effort increases are rate-limited; reductions are immediate. A motoring/regen sign change passes through zero for one scheduled tick. Zero charge permission removes regen immediately.

The policy matches the C prototype. The runtime adds restrictions rather than
changing that comparison contract. Current allocation remains a linear simulation
envelope, not a measured torque or battery-current controller.

## Runtime integration contract

1. Value-initialize all structures. Call `initialize(clock_ms, state, port)` before acquisition; it requests inhibit. Treat a false result as a driver fault.
2. Supply a coherent, validated `Snapshot`. Validate real telemetry CRC/counters, units and source mapping before refreshing timestamps. Pedal/direction validation and energy-management completion must feed permission/`energy_ready`.
3. Use the runtime's monotonic acquisition clock: `state.monotonic_ms + uint32(clock_ms - state.last_clock_ms)`. Do not stamp an old sample when it is consumed; retain its actual acquisition time. The 32-bit hardware clock may wrap normally. A backward jump or excessive gap faults.
4. Call `run` from the scheduled task. Calls before the period do no I/O and do not feed the watchdog. A missed deadline latches a fault. An independent hardware watchdog must still act if this task stops altogether.
5. Implement `Hardwareport` with bounded calls. Mapping must be calibrated per motor, including zero effort, direction and regen; there is no default effort-to-throttle formula. Both payloads are prepared before either is sent. Any failed dispatch requests inhibit for both machines.

`send=true` means accepted by the local driver, not delivery or execution by an
inverter. `setinhibit=true` acknowledges a driver request, not measured gate state.
The watchdog and inhibit calls are tested with mocks only. Inhibit must be effective
during boot, reset and a stalled processor independently of this library.

The configuration rejects duplicate/invalid standard CAN IDs, invalid periods,
nonfinite slew limits and timing budgets over 100 ms. It requires:

`max(motor stale threshold, BMS stale threshold) + maximum task interval + inhibit budget <= 100 ms`

The test configuration uses 80 + 10 + 10 ms. These are timing allocations, not
measured guarantees. The complete acquisition, scheduling, execution and physical
inhibit path must fit that budget; port blocking and execution time count toward
the final term. No production runtime configuration is supplied. A zero-initialized
configuration fails closed.

## Build and reproduce

Normal Windows build:

```powershell
cmake -S . -B build-capstone -G "Visual Studio 17 2022" -DBUILD_TESTING=ON
cmake --build build-capstone --config Debug
ctest --test-dir build-capstone -C Debug --output-on-failure
.\build-capstone\Debug\dual_control_demo.exe
```

For the actual C++03 parser and compilation database, run from a Windows x64
developer terminal with Clang and Ninja installed:

```powershell
cmake -S . -B build-control-clang -G Ninja `
  -DCMAKE_C_COMPILER="C:/Program Files/LLVM/bin/clang.exe" `
  -DCMAKE_CXX_COMPILER="C:/Program Files/LLVM/bin/clang++.exe" `
  -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build build-control-clang --target cpp_control_tests
ctest --test-dir build-control-clang -R cpp_control_regression --output-on-failure
python tools/run_jsf_review.py `
  --checker "../../smash/jsfav-checker/build-msvc/Release/jsfav.exe" `
  --compdb build-control-clang
python tools/review_control_metrics.py --compiler "C:/Program Files/LLVM/bin/clang++.exe"
```

The control target disables exceptions, RTTI and fast-math, and treats compiler
warnings as errors. MSVC lacks a C++03 language switch; the Clang build verifies
that syntax. Host tests may use a newer language mode and are outside the JSF
scope. Linux portability has not been tested in this session.

The console demo shows combined commands, MG1 feedback loss, continued inhibition
after feedback returns, deliberate disarmed reset and re-arming. Its frame IDs,
calibration and all feedback are illustrative. It never accesses a CAN device.

## Remaining work

Complete the manual JSF rule review and resolve platform/toolchain dispositions,
especially `stdint.h`, numeric behavior, interfaces and measured timing. Then select
the MCU/board revision, implement the hardware port, verified telemetry decoders
and the actual precharge/contactor/discharge state machine. Build and inspect an
MCU image before logic-only fault tests and individual motor commissioning.

See [JSF evidence](JSF_REVIEW.md), [source scope](../../configs/control-source-scope.json)
and [control review](CONTROL_REVIEW.md).

## Examiner readability review

See [CODE_REVIEW.md](CODE_REVIEW.md) for the current source map and repeatable
format/token/AST checks. Comments distinguish host/current-envelope assumptions
from measured hardware behavior. This documentation pass does not extend the
JSF scope to the Arduino subsystem or establish manual-rule compliance.
