# Parking-pawl integration review

## Current baseline

- Vehicle: originally manual 1996 Civic DX; owned 2009 Prius transaxle.
- Exact actuator, connector view/pigtail and final MCU/driver remain unverified.
- The sketch pin assignments, current thresholds and timing values are examples.
- Both publication builds disable motor power and phase PWM by default.

## What the drafts implement

The bench sketch provides bounded serial jog/move commands, current measurement,
Hall-progress checks, limits and fault handling. The vehicle sketch checks ARM,
brake, two zero-speed contacts, request validity and independent PARK/NOT-P inputs;
it includes current, Hall-progress and move-time limits.

These are source-code features, not measured safety guarantees. Motion uses timed
phase stepping with Hall activity monitoring; it is not validated Hall-commutated
closed-loop position control. CAN messages are proposed notes, not implemented
communication or an approved vehicle protocol.

## Required work before vehicle use

1. **Hardware:** verify connector orientation and continuity, supply/logic levels,
   motor topology, driver ratings, current calibration and inductive protection.
   MCU pins never connect directly to motor phases or unconditioned vehicle wires.
2. **Manual-car controls:** define explicit park/release requests and invalid states;
   do not assume the manual Civic contains the inherited automatic-shifter contacts.
3. **Traction handshake:** prohibit torque while parked, moving the actuator,
   position-unknown or faulted. Permit drive only with confirmed NOT-P and healthy
   interlocks. The sketches do not implement this complete VCU handshake.
4. **Speed and position:** verify independent zero-speed evidence with loss/stale/
   stuck-signal behavior. Two asserted contacts alone do not prove fresh zero speed.
   Calibrate physical PARK/NOT-P sensing; Hall transitions show progress only.
5. **Recovery:** add deliberate rearming and a fresh request after reset/fault.
   The inherited level-request loop may move again after service clearing or reboot
   if its inputs permit it. `clearFaultIfSafe()` checks ARM, not a complete recovery
   policy. Do not describe it as restart-safe.
6. **Independent protection:** review watchdog/brownout/transient behavior and
   hardware defaults. Motor-power removal stops commanded actuation; it does not
   prove that an engaged pawl released or that the vehicle is secured.

## Evidence gates

| Gate | Required record |
|---|---|
| Compile | Pinned toolchain, both target results and source revision |
| De-energized/logic | Harness continuity, driver defaults and interlock truth table |
| Guarded bench | Direction, current, Hall sequence, limits, travel and stopping behavior |
| Fault injection | Brake/speed/ARM loss, request conflict, position conflict, jam/current, timeout, reset and power loss |
| Vehicle | Reviewed traction handshake, stationary park/release, holding and manual recovery with mechanical brake retained |

Do not engage the pawl during a moving-vehicle fault test. No such tests are
authorized or recorded by this publication. Preserve unknown state and inhibit
traction until position and recovery conditions are verified.
