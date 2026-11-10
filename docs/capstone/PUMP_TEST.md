# Confirm the coolant-pump interface

**Recommended order: identify the wiring, then perform a wet bench test.**
The four-pin connector shape and BMW application number do not resolve the
conflicting pinouts. Do not apply power to unknown pins to find out.

## Testing options

| Option | What to do | What it confirms |
| --- | --- | --- |
| **1. Exact-unit documentation** | Obtain the pump maker/model and its connector diagram from the supplier. Match molded cavity numbers, latch orientation, supply pins, protocol and fuse requirement. | Approved starting point for wiring; does not prove the pump works. |
| **2. Wet bench test — preferred** | After option 1, test your pump with a suitable supply, verified harness and a controller confirmed for that exact version. | Startup, current draw, speed response, flow and communication-loss behavior. |
| **3. Matching donor vehicle or specialist** | Have a technician inspect the correct donor harness and capture the pump's supply/control signals while commanding it through diagnostics. Confirm the pump/version matches yours. | Actual harness pin roles and signaling; useful when supplier information is missing. |

A generic CWA200 or BSD diagram is a reference, not approval for the pictured
replacement. A donor measurement does not automatically prove an aftermarket
unit behaves identically.

## Wet bench test

### Equipment

- Verified connector/pigtail; appropriate wire and fuse from the exact-unit specification.
- Suitable adjustable, current-limited DC supply with current display and accessible off switch.
- Filled reservoir, compatible coolant, hoses and clamps; pump mounted below fluid level.
- Matching pump controller, flow meter and thermometer. A DC clamp meter is useful.
- Oscilloscope/appropriate interface probe if identifying protocol or checking command signals.

### Procedure and acceptance

| Step | Test | Record / acceptance |
| --- | --- | --- |
| 1 | Confirm connector numbers and polarity with power disconnected. Identify inlet/outlet. Fill and bleed the loop. | Diagram/harness match; flooded pump; no leaks. |
| 2 | Power using the specified supply and command sequence. Begin with a conservative current limit; raise it only within documented limits. | Supply voltage at pump, current, startup delay and continuous flow. A low current limit can cause brownout: no-start alone is inconclusive. |
| 3 | Request low, medium and high speed using the matching controller. | Repeatable flow changes and stable supply/current. These are controller speed requests, not assumed raw PWM duty cycles. |
| 4 | At a stable condition, remove only the identified control signal. Observe from a safe position with immediate power-off available. | Actual stop, full-speed, hold-last-speed or delayed response; record timing. Use a bounded observation window and report it if no response occurs. |
| 5 | Restore control, command stop, and repeat startup/power cycling. | Repeatable recovery and predictable stop/start behavior. |
| 6 | Repeat with the intended radiator, inverter, transaxle and hoses installed in the loop. | Installed flow, voltage/current, coolant inlet/outlet temperatures and leaks. The current model assumes 10 L/min total; this is a design assumption, not a verified minimum for every branch. |

**Stop on leakage, loss of prime/flow, abnormal heating/noise or excessive current.**
Do not run dry or deliberately block the outlet. Do not use supply chopping as a
substitute for the pump's specified control interface.

## Interpreting the result

- **Pump spins:** confirms operation, not adequate coolant flow or protocol identity.
- **Runs without control:** confirms observed fallback only for that unit and test condition.
- **Flow follows commands:** supports controller compatibility; waveform capture is needed to identify the protocol itself.
- **No response:** check pinout, voltage at the pump, current limiting, priming and controller compatibility before calling the pump faulty.
- **Installed flow passes:** proceed to loaded thermal testing; it does not establish continuous motor capability.

Never treat BSD and LIN as interchangeable. Choose the interface circuit after
identifying the protocol. Do not base drive authorization solely on expected
full-speed fallback; verify coolant circulation independently.

## Test record

Pump maker/model/date: ___  Diagram/source: ___  Harness/cavity orientation: ___

Supply/fuse/current limit: ___  Controller/protocol: ___  Coolant/temperature: ___

| Condition | Pump voltage | Current | Flow | Delay / response |
| --- | --- | --- | --- | --- |
| Startup | | | | |
| Low request | | | | |
| Medium request | | | | |
| High request | | | | |
| Signal removed | | | | |
| Control restored | | | | |
| Installed loop | | | | |

Result: **confirmed / failed / inconclusive**. Remaining unknowns: ___
