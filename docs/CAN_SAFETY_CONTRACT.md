# Shared CAN safety contract

The dual-motor extension is specified in [CAN_INTERFACE.md](capstone/CAN_INTERFACE.md).
MG1/MG2 use independent synthetic feedback and a 100 ms stale boundary. Production
CAN encoding is an offline utility only; no synthetic profile is hardware-verified.

This repository is a bench simulator. It has no real CAN transmitter, inverter driver, charger output, contactor output, or Civic diagnostic output.

The simulated CAN bus is deliberately untrusted. Messages can be delayed, dropped, duplicated, corrupted, replayed, or blocked by bus-off. Each node reports a heartbeat and the supervisor validates freshness, DLC, sequence, and CRC before using data.

## Node responsibilities

| Node | Publishes | Receives |
| --- | --- | --- |
| Leaf LBC/BMS | pack voltage/current, SOC/SOH, temperatures, cell extrema, charge/discharge limits, contactor/interlock status | none in v1 |
| Prius/OpenInverter inverter | speed, current, temperatures, ready/fault/desaturation state | bounded torque, direction, enable |
| Tesla OBC | AC/DC state, output limits, thermal/fault state | bounded charge command |
| Safety supervisor | authorization and safe-state decisions | all safety-relevant node status |

The supervisor is the only simulated node that can authorize torque, charging, or contactor transitions. A CAN frame never replaces an independent emergency-stop, HVIL, contactor-feedback, or current/voltage protection path.

The CAN IDs in the simulator (`0x180`, `0x280`, and `0x380`) are test IDs only. They are not claimed to match a Nissan Leaf, Tesla, Toyota, or OpenInverter production map.

## Fail-safe rules

- A missing or invalid BMS frame inhibits both drive and charge.
- A missing inverter heartbeat inhibits torque.
- A missing charger heartbeat inhibits charge.
- Bus-off, invalid CRC/DLC, replay, and contradictory node state are faults.
- Emergency stop, HVIL, insulation, overcurrent, overspeed, inverter, charger, and contactor faults latch.
- Fault reset is not exposed by the GUI and requires all safety inputs to be healthy in the core API.

Use the simulator to test logic and integration. Obtain independent review and perform low-energy, isolated bench validation before connecting any high-voltage equipment.
