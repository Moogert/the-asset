# Control software: first review and next implementation

> **29 October controller revision:** see [current controller and topology](CONTROLLER_RELEASE.md). The default Teensy now coordinates Honda-induced faults and USB details; Honda owns OBD/MIL. Earlier combined-gateway results describe the isolated legacy demonstration.


27 October 2026. Status: tested C++ supervisor/runtime library and C11 reference
harness; no embedded hardware control. See [the implementation guide](CONTROL_CPP.md).

## Architecture to review

MG1 and MG2 are separate machines. Each requires its own resolver, phase-current
feedback and FOC loop. The supervisor coordinates their permitted effort and
shared battery limits. The selected mechanical configuration makes their signed
speeds equal after sensor-sign calibration.

| Layer | Current implementation | Next deliverable |
| --- | --- | --- |
| Motor FOC | Pinned upstream source identified | Build exact V1d targets; document board-dependent PWM, resolver and sensor configuration |
| Supervisor policy | C++03 port; 6,494 comparisons with C11 reference | Complete manual JSF review and MCU integration |
| Controller adapter | C++ checked encoder; mocked dispatch and independent counters | Verified telemetry, calibrated mapping and physical transport |
| Energy management | Simulated authorization flags | Precharge, contactor feedback, discharge and fault sequences for actual circuit |
| Hardware protection | Abstract inhibit/watchdog interfaces and failure tests | Physical implementation and measured total response time |

The host UI and Python models remain development tools. The new C++ compliance
claim should apply to an enumerated embedded source set, not silently extend to
all upstream dependencies. Professor acceptance of that scope remains pending.

## Behavior contract

- Both feedback channels must be healthy and fresh in every mode, including single-motor tests.
- Start requires valid pedal/direction, battery permission, ready channels and completed energy-management checks.
- A fault zeros both commands and removes drive authorization. A healthy, disarmed, zero-request reset is required before re-arming.
- Mode changes occur while disarmed. Commands use a documented sign convention: positive motoring, negative regeneration; direction is handled separately by the hardware adapter.
- Shared discharge and charge budgets constrain allocation. Actual current monitoring must detect overload independently of the predicted allocation.
- No charge permission means zero regenerative request. Overspeed and temperature limits apply separately to each motor.
- A 10 ms scheduled task checks feedback ages and emits independent checked commands. A watchdog/inhibit path must cover stalled execution.

The existing policy detects stale feedback at 100 ms. A periodic scheduler and
physical interface add latency: to meet a total <=100 ms target, budget a shorter
embedded detection threshold and measure worst-case scheduling, bus and inhibit
delays. The host test at the exact threshold is not a physical timing result.

## Implementation order

1. **Freeze interfaces.** Select actual V1d revision and supervisor hardware; review the internal CAN modification. Specify node IDs, telemetry source/units, CRC/counter behavior, stale deadlines and independent inhibit wiring. Keep hardware outputs disabled until these are reviewed.
2. **Review the C++ port.** The library now separates configuration, input snapshot, persistent state and output request. Its interfaces use fixed-size data; compiler settings disable exceptions/RTTI and fast-math. Complete manual rule dispositions and target-specific resource/numeric review.
3. **Extend integration evidence.** C/C++ comparison and mocked runtime tests now cover modes, budgets, faults, BMS freshness, reset restrictions, clock wrap, command slew and overruns. Next test actual telemetry decoders with captured frames, including missing, replayed and out-of-order messages.
4. **Build the hardware layer.** Add transport, periodic scheduling, watchdog, energy management and fail-off output behavior. Do logic-only tests first. Record measured fault-to-inhibit latency, including a frozen or reset supervisor.
5. **Commission sequentially.** MG2, then MG1, then combined low-load operation. Calibrate throttle-digit/effort maps and shared pack-current limits before torque optimization. Verify that the inactive channel remains mechanically safe at speed.

## First-review evidence

The five project CTest suites pass on Windows/MSVC Debug. Coverage includes
2,091 allocation combinations, independent motor fault injection, stale timing,
latched recovery, offline CRC/message encoding, ten physics-model tests and a
loopback HTTP integration test, plus C++ comparison/runtime tests. The C++ library
also builds under Clang C++03. The literal checker runs all 40 automated checks
with zero reported violations, no coverage warnings and 193 manual review items.
See `VALIDATION.md` for reproduction and limits.

Simulation constants returned by `dual_simulation_config()` are not hardware
ratings. The current linear allocation cannot guarantee phase-current limits,
torque accuracy, battery compliance or optimum efficiency. Those require measured
maps and motor-controller limits. No loaded torque or physical shutdown test is
complete.

## Review decisions and completion criteria

For the first review, request feedback on the dual-channel architecture, selected
fault behavior, compliance scope and measurement plan. For the next software
review, the compiling C++ target, comparison tests, build commands, checker
JSON/SARIF and 233-rule worksheet are now available. Manual rule dispositions,
professor scope acceptance and target integration remain open. Do not label the
result compliant while required reviews remain open.

See [JSF evidence](JSF_REVIEW.md), [CAN specification](CAN_INTERFACE.md),
[requirements](REQUIREMENTS.md) and [commissioning](COMMISSIONING.md).
