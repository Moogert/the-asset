# Custom CAN Integration Notes

These notes are for the 1996 Civic DX / Prius transaxle swap context.

## Key Point

The 1996 Civic ECU should not be expected to appear directly on a CAN bus. It is an OBD-II-era ECU, but pre-2008 OBD-II vehicles commonly used non-CAN diagnostic protocols. Treat the Civic ECU and body wiring as legacy discrete/diagnostic I/O, then bridge only the signals you need into your custom CAN bus.

That means the clean architecture is:

```text
Civic switches / ECU wires / sensors
        |
protected inputs and relay/MOSFET outputs
        |
CAN gateway node
        |
custom CAN bus
        |
VCU / dash / pawl controller / thermal controllers
```

For the parking pawl, CAN is useful as a supervisory channel. It should not be the only safety path. Keep local hardwired inputs for brake, zero-speed, actuator position confirmation, and motor-power enable.

## Civic A/C Signals

The 1996 Civic A/C system is a good candidate for gatewaying. Service information describes the A/C request path as a hardwired request through the A/C switch, blower switch, thermostat, and pressure switch into the ECM/PCM. The ECM/PCM then grounds the compressor clutch relay after its own delay/load handling.

Recommended approach:

- Read the Civic A/C request as a protected input.
- Preserve the pressure switch / thermostat safety chain.
- If the original Civic ECU is still running the engine, let it keep authority over compressor clutch relay timing.
- If the engine ECU is gone or no longer managing idle/load, your CAN gateway or VCU can publish `AC_REQUEST`, `AC_ALLOWED`, and `COMPRESSOR_ENABLE`, but the final compressor output should still pass through refrigerant pressure protection.

Do not put compressor clutch, condenser fan, or high-current HVAC loads directly on Arduino pins. Use automotive relays, protected low-side drivers, or high-side switches.

## Parking Pawl CAN Policy

For the pawl actuator, I would use CAN like this:

- Allowed CAN inputs: `vehicle_speed`, `drive_enabled`, `brake_status`, `shift_position`, `vcu_alive`.
- Hardwired required inputs: brake-ok, zero-speed-ok, PARK confirmation, NOT-P confirmation, ARM enable, motor-power cutoff.
- CAN outputs from the pawl controller: `pawl_state`, `pawl_fault`, `pawl_current`, `hall_edges`, `park_confirmed`, `not_p_confirmed`.

If a CAN signal disagrees with a hardwired safety signal, fault and remove actuator motor power.

## Draft CAN Messages

These are placeholders until you define your bus speed, IDs, byte order, and owning controller.

| Message | Direction | Suggested contents |
| --- | --- | --- |
| `VCU_Status` | VCU to nodes | alive counter, drive enabled, vehicle speed, brake status |
| `Shift_Status` | shifter/gateway to bus | Civic P selected, Civic not-P selected, brake switch, ebrake switch |
| `Pawl_Status` | pawl controller to bus | state, fault code, current, Hall edge count, park/not-P confirmed |
| `HVAC_Request` | Civic gateway to VCU | A/C button, blower active, pressure switch ok, evap thermostat ok |
| `HVAC_Command` | VCU to gateway | compressor enable, condenser fan request, load-shed request |

Use an alive counter and timeout on every command-like message. A stale CAN message should become "unknown/off", not "keep doing the last thing forever."

## Hardware Suggestions

For an Arduino-era prototype:

- Native-CAN microcontroller board if possible, such as Teensy 4.x plus a CAN transceiver, Arduino Due plus transceiver, or an ESP32/TWAI board with an automotive CAN transceiver.
- If using Uno/Nano, an MCP2515 CAN module can work for lower-rate prototyping, but choose an 8 MHz vs 16 MHz module intentionally and verify library timing.
- Use a real CAN transceiver, twisted pair, 120 ohm termination at the bus ends, and common grounding strategy.
- Add input protection for every wire leaving the enclosure: series resistance, clamps/TVS, reverse battery protection, and load-dump tolerant power supply.

## Sources

- 1996 Civic A/C service description: ECM/PCM receives cooling demand, delays compressor engagement, and enriches/handles transition to A/C mode: https://workshop-manuals.com/honda/civic_coupe/l4-1.6l_sohc_%28vtec%29/heating_and_air_conditioning/compressor_clutch_relay/component_information/locations/page_7127/
- 1996 Civic A/C pressure switch / relay description: A/C request path through switch, blower, thermostat, pressure switch; ECM/PCM grounds compressor clutch relay: https://workshop-manuals.com/honda/civic_coupe/l4-1.6l_sohc_%28vtec%29/heating_and_air_conditioning/refrigerant_pressure_sensor/switch_hvac/component_information/locations/page_7344/
- OBD-II CAN became mandatory on US vehicles later than this Civic generation: https://www.csselectronics.com/pages/obd2-explained-simple-intro
