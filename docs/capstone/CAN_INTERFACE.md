# CAN and supervisor integration contract

## Separation of interfaces

The new [C++ runtime](CONTROL_CPP.md) adds an abstract two-channel transport,
per-channel counters, calibrated mapping contract and watchdog/inhibit requests.
Its console demo and tests use mock ports. No real CAN driver, telemetry decoder,
command calibration or physical inhibit implementation is supplied.

1. The localhost harness uses **synthetic telemetry** and never transmits CAN.
2. `oi_encode_control()` builds offline v5.40.R-compatible payload bytes only.
3. Production node configuration, telemetry and physical CAN integration remain
   unverified until exact hardware is inspected. Do not flash the worksheets.

Pinned upstream source:
<https://github.com/jsphuebner/stm32-sine/tree/5adabd9fdd382ca8c7076174a390e8977ee9066e>.
Receiver: `src/vehiclecontrol.cpp`; parameters: `include/param_prj.h`.
ZombieVerter `src/Can_OI.cpp` at the locked reference is a starting point; its
SetTorque is unused and it sends throttle digits, not Nm.

## Production control proposal

MG1 node 1/control ID 0x03F; MG2 node 2/control ID 0x040; 500 kbit/s, 10 ms period.
These are **project proposals**, not OEM IDs or claims about the boards as supplied.
Check conflicts and configure each processor separately. Feedback IDs/scales remain
unset until upstream CAN mapping and the actual board are validated.
Set unique SDO node IDs as well as distinct command/telemetry IDs; changing nodeid
alone does not give each motor an independent controlid.

An 8-byte control payload contains two little-endian 32-bit words:

| Bits | Meaning |
| --- | --- |
| Word 0, 0-11 | Throttle A raw digits, 0..4095 |
| Word 0, 12-23 | Throttle B raw digits, 0..4095 |
| Word 0, 24-29 | Cruise/start/brake/forward/reverse/BMS bits |
| Word 0, 30-31 | Two-bit counter |
| Word 1, 0-13 | Cruise speed, zero in the offline encoder |
| Word 1, 14-15 | Same counter |
| Word 1, 16-23 | Regen preset, limited to 0..100 here |
| Word 1, 24-31 | Low byte of STM32 CRC over both words, CRC field zeroed |

CRC uses polynomial 0x04C11DB7, initial 0xFFFFFFFF, MSB-first 32-bit words,
no reflection and no final XOR. Word bit processing order differs from byte
serialization order. This is **not** the harness's polynomial-0x1D synthetic CRC8.
Maintain a separate rolling counter per channel. Throttle zero, channel polarity,
regen behavior, calibration and direction all depend on that processor's parameters;
zero raw digits must not be assumed to mean zero torque.

The offline encoder is not connected to `dual_drive_evaluate()`: converting a
normalized effort request to throttle digits needs measured per-channel mappings,
particularly for regen and reversing. Do not bridge them with an assumed linear Nm scale.

Native timeout is about 500 ms. The pinned upstream receiver can update command
values during tolerated CRC/counter errors before its error count reaches the
threshold. CRC alone is not a 100 ms fail-safe mechanism. Strict local feedback
validation, physical inhibit and a separately measured watchdog are required.
Two-bit counters provide limited sequence checking, not replay authentication.

## Physical interface work order

The original V1d has an internal transceiverless connection between STM32s, with
an externally connected Atmel CAN participant described in the support thread.
Do not attach CAN-H/CAN-L to the internal logic net.

For the actual board revision, record the schematic and board photos, trace
each MCU CAN TX/RX net and the diode/pullup network, and design two proper CAN
physical interfaces. Isolate/remove/adapt the transceiverless network as the
reviewed schematic requires. Confirm logic voltages, supply budget, termination,
CAN common-mode range, grounding/isolation, ESD and fail-off outputs.
Verify this does not alter hardware-identification straps or shared fault logic.

No MCU pin numbers, resistor removals or jumper positions are prescribed here:
the physical revision is unknown. Record approved locations and continuity checks
in an as-built drawing before soldering. Measure reset, bootloader, brownout,
unpowered node, stuck-dominant and bus-off behavior without motor power.

Port the portable policy into a 10 ms ZombieVerter task with two independent
feedback/control instances. Replace the synthetic decoder with verified CAN maps.
Use actual elapsed monotonic time, not loop counts. Use actual pack current and
BMS dynamic limits; predicted current envelopes are only allocation estimates.
Independent watchdog/inhibit must work if the MCU scheduler or CAN peripheral stops.

## Synthetic harness telemetry

MG1 0x281/source CAN_NODE_MG1; MG2 0x282/source CAN_NODE_MG2. Both DLC 8.

| Byte | Encoding |
| --- | --- |
| 0 | Upper nibble sequence modulo 16; low bits: ready=1, resolver valid=2, fault=4; bit 3 reserved |
| 1-2 | Signed int16 shaft rpm, little-endian |
| 3 | Motor temperature +40 C, integer |
| 4 | Inverter temperature +40 C, integer |
| 5-6 | Signed int16 estimated/measured channel DC current in 0.1 A, little-endian |
| 7 | Synthetic CRC8 over bytes 0..6 |

Strict DLC/source/reserved-bit/CRC/next-counter checks precede state updates.
Reject future timestamps and samples 100 ms old. Timestamps are simulation metadata,
not real CAN payload fields. A dropped or invalid frame may require a new session
because sequence discontinuities latch an inhibit. Physical CAN does not inherently
authenticate source identities or provide these simulation timestamps.

Existing BMS 0x180, legacy single-inverter 0x280 and charger 0x380 remain synthetic.
The two motor messages are authoritative for dual-channel feedback; legacy status
remains an additional compatibility gate. No synthetic ID is a production profile.

## Policy and limitations

Both connected machines must be healthy and fresh, even in single-motor test mode.
Healthy startup defaults to MG2-only; mode changes while armed/drive requested latch.
All fatal dual faults latch; reset requires healthy inputs, disarmed, no drive request
and zero effort. Fault history survives removal of the triggering fault.

Positive effort is motoring; negative is regeneration. Hardware forward/reverse
is a separate calibrated direction function, not the sign of this effort value.
Channel thermal factors apply before proportional allocation of the shared DC budget.
Actual pack overcurrent and opposite-signed channel DC currents above the simulation
tolerance inhibit both. There is no current-loop plant or torque-speed dynamics model.

The legacy aggregate `torque_percent` is the largest-magnitude normalized channel
command, not the sum or Nm. Use the `dual` JSON object for separate commands.
`contactor_command=false` is a simulated authorization state, not an approved
high-speed HV opening sequence. Validate hardware shutdown with the power stage,
back-EMF, bus energy and contactor operating limits.
