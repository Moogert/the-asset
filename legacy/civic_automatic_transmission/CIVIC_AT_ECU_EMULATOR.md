# Civic Automatic ECU Signal Emulator

**Historical automatic-transmission reference.** The current vehicle is reported
as an original manual DX. Use the separate [engine emulator and unified diagnostic
work package](../../docs/capstone/HONDA_ECU_AND_OBD.md); the defaults below are not a
verified manual-car engine/harness configuration.

This note pairs with `CivicAutoTransSignalEmulator.ino`. It is aimed at the current project context: keeping a 1996 Civic automatic-transmission PCM/gauge harness satisfied while the vehicle speed source comes from the EV/motor CAN bus.

## Scope

The sketch emulates the transmission-side signals that are normally inputs to the Civic PCM or gauge harness:

| Signal | Firmware behavior | Hardware interface |
| --- | --- | --- |
| VSS | MPH-to-pulse train, default 4000 pulses/mile | Open-drain/opto pull-down to the Civic VSS line |
| Mainshaft speed sensor | Pulse train derived from MPH, final drive, tire diameter, and virtual gear ratio | Open-drain/opto pull-down |
| Countershaft speed sensor | Pulse train derived from MPH and final drive | Open-drain/opto pull-down |
| Transmission range switch | Active-low switch closures for P/N, R, D4, D3, 2, optional 1 and P | One protected low-side switch per PCM range input |

The sketch intentionally treats PCM-to-transmission solenoid wires as hardware, not firmware:

| PCM output | What to do |
| --- | --- |
| Shift solenoid A/B | Use rated dummy loads or the original solenoids mounted remotely |
| Lock-up control solenoid valves | Use rated dummy loads or original solenoids |
| Linear solenoid | Use a rated load emulator designed for duty-controlled current |

A microcontroller input can watch those PCM outputs through optocouplers if useful, but it should not be wired directly to them and it cannot remove an open-load fault unless the electrical load is really present.

## Default CAN Frame

The sketch defaults to a simple placeholder frame:

| Field | Default |
| --- | --- |
| CAN ID | `0x201` |
| Speed | bytes 0-1, little-endian unsigned, `0.01 mph/bit` |
| Gear | byte 2, optional |

Gear byte values:

| Byte | Range |
| --- | --- |
| `0` | P |
| `1` | R |
| `2` | N |
| `3` | D4 |
| `4` | D3 |
| `5` | 2 |
| `6` | 1 |

To use an MCP2515 CAN module, install MCP_CAN_lib, set `USE_MCP2515_CAN` to `1`, and verify the module oscillator. Many modules are 8 MHz, while the sketch default is `MCP_16MHZ`.

## Default Pin Map

| MCU pin | Purpose |
| --- | --- |
| D2 | VSS pulse output |
| D3 | Mainshaft pulse output |
| D4 | Countershaft pulse output |
| D5 | P/N range switch output |
| D6 | R range switch output |
| D7 | D4 range switch output |
| D8 | D3 range switch output |
| D9 | 2 range switch output |
| A0 | Optional 1 range switch output |
| A1 | Optional Park-only range output |
| D10 | MCP2515 CS |
| D11-D13 | SPI for MCP2515 |
| A2 | MCP2515 INT |
| A3 | Emulator enable input, active low |
| A4 | Fault output |
| A5 | Status LED output |

Every output assumes it drives an external protected pull-down stage. Do not connect these pins directly to Civic PCM wires.

## Calibration Items

These constants in the sketch are the first things to verify:

| Constant | Why it matters |
| --- | --- |
| `VSS_PULSES_PER_MILE` | Cluster and PCM speed scaling |
| `TIRE_DIAMETER_IN` | Shaft-speed model |
| `FINAL_DRIVE_RATIO` | Shaft-speed model |
| `GEAR_RATIO_1` through `GEAR_RATIO_4`, `GEAR_RATIO_R` | Mainshaft-speed plausibility |
| `SHIFT_1_TO_2_MPH`, `SHIFT_2_TO_3_MPH`, `SHIFT_3_TO_4_MPH` | Virtual gear used while range is D4/D3 |
| CAN ID/byte layout constants | Match your actual VCU/motor controller bus |

If the PCM still sets ratio, shaft-speed, or solenoid electrical DTCs, capture the PCM pin voltages and the generated waveforms with a scope before changing code. For the shaft sensors, it is better to tune frequency relationships from measured service-manual expectations or a known-good automatic car than to guess.

## Bring-Up

1. Bench-test with the serial override before connecting to the car. Send `M25.0`, then check D2/D3/D4 with a scope or logic analyzer.
2. Verify each range output with a meter. Only one range closure should be active at a time, except P may activate both the P/N line and optional Park-only line.
3. Connect outputs through optocouplers or transistor stages only.
4. Enable CAN and confirm stale CAN causes the speed outputs to stop and `FAULT_OUT` to activate.
5. Add solenoid dummy loads separately, with fusing and temperature checks.

## Vehicle-Specific Open Questions

These are the details that would let this become a precise pin-for-pin harness design:

- Exact PCM part number, for example the full `37820-P2E-...` code.
- Whether this is non-HX 4 A/T or HX CVT wiring. The HX CVT has different speed sensors and solenoids.
- Whether you want the original Civic shifter/range switch to remain connected, or whether the emulator should own all range-switch outputs from CAN.
- Whether the Civic PCM still controls anything important, such as A/C request, radiator fan, tach/speedometer, or diagnostics only.

## References

- Project notes already captured the 1996 Civic VSS as a 5 V-class pulse line that is pulled low by the VSS.
- The public 1996 Civic LX transmission wiring index lists the non-HX automatic items this sketch targets: VSS/VSS out, mainshaft and countershaft speed sensors, A/T gear position switch, shift solenoids, lock-up solenoids, and linear solenoid.
- Honda OBD2A pinout references are useful for connector naming, but verify against the exact PCM and harness because automatic/manual and HX/non-HX variants differ.
