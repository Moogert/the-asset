# Gen 2 Prius Parking Pawl Actuator Controller Notes

> **10 November 2026 review:** the Civic is originally manual. Automatic-shifter
> contacts below are inherited assumptions, not verified installation inputs.
> Both pawl sketches now set `ENABLE_ACTUATOR_OUTPUTS = false` by default.
> Review the publication package's integration gaps before considering activation.
> The sketches are development references, not a validated vehicle safety system.

This package is for characterization and early control design for a 2004-2009 Toyota Prius shift control actuator / parking pawl actuator.

The actuator can physically lock the driveline. Treat it like a safety-critical brake/gearshift component: use wheel chocks, an emergency stop, a fused current-limited supply, and do not test on a vehicle that can roll.

## What The Research Says

The Gen 2 Prius electronic shift lever system controls park electrically. Toyota's repair information describes the shift control actuator as a parking lock motor plus a rotation-angle sensor. The motor output is reduced through a cycloid mechanism, and the sensor uses 3 Hall ICs. The transmission control ECU applies current to the motor based on the Hall sensor signals.

The system-level behavior is also important. To engage park, the P switch request is sent to the hybrid vehicle control ECU, which checks whether park is allowed before the transmission control ECU drives the actuator. To release park, the vehicle must be ready to drive and the brake pedal is involved. Toyota's reject logic holds P if the brake is not depressed, and rejects a park request while driving by changing to N.

The actuator is therefore not a 2-wire reversible motor. It is effectively a 12 V common-feed, 3-phase low-side-switched actuator with Hall feedback.

Primary references used:

- 2006 Prius repair manual, electronic shift lever system and actuator description: https://attachments.priuschat.com/attachment-files/2020/07/190278_2nd_gen_Transaxle.pdf
- 2006 Prius wiring diagram manual, S1 shift control actuator wiring and connector part number: https://attachments.priuschat.com/attachment-files/2020/06/188209_prius_wiring.pdf
- Mirrored 2006 Prius C2300/C2304/C2307 diagnostic pages with connector cross-checks: https://dokument.pub/2300-flipbook-pdf.html
- Luscious Garage teardown video showing the Gen 2 actuator internals: https://www.youtube.com/watch?v=BmCgtPijXvQ
- 1996 Civic DX shift interlock diagram index, showing brake switch, A/T gear position switch, parking pin switch, interlock control unit, and shift lock solenoid: https://portal-diagnostov.com/cs/2020/04/07/shift-interlocks-honda-civic-dx-1996-system-wiring-diagrams/
- 1996 Civic hatchback VSS description, showing a 5 V pulled-up BLU/WHT speed signal that is pulsed to ground by the VSS: https://workshop-manuals.com/honda/civic_hatchback/l4-1.6l_sohc_%2816_valve%29/powertrain_management/computers_and_control_systems/vehicle_speed_sensor/component_information/diagrams/page_3822/
- 1996 Civic A/C service description, showing ECM/PCM involvement in compressor relay timing and A/C load handling: https://workshop-manuals.com/honda/civic_coupe/l4-1.6l_sohc_%28vtec%29/heating_and_air_conditioning/compressor_clutch_relay/component_information/locations/page_7127/
- 1996 Civic A/C pressure switch and relay description, showing hardwired request/safety chain and ECM/PCM relay grounding: https://workshop-manuals.com/honda/civic_coupe/l4-1.6l_sohc_%28vtec%29/heating_and_air_conditioning/refrigerant_pressure_sensor/switch_hvac/component_information/locations/page_7344/

## Actuator Connector Working Map

This is the Gen 2 S1 connector map from the available Prius wiring/repair information. Verify against your exact harness before energizing anything.

| S1 pin | Toyota symbol | Function | Suggested controller connection |
| --- | --- | --- | --- |
| 1 | MWA | W phase motor lead | Low-side MOSFET W drain |
| 2 | MGNA | Motor common supply | Fused switched +12 V |
| 3 | RB | Hall/angle sensor output B | Arduino digital input |
| 4 | RZ1 | Hall/angle sensor output Z1 | Arduino digital input |
| 5 | RA | Hall/angle sensor output A | Arduino digital input |
| 6 | MVA | V phase motor lead | Low-side MOSFET V drain |
| 7 | MUA | U phase motor lead | Low-side MOSFET U drain |
| 8 | RE2 | Sensor ground | Logic ground |
| 9 | RVC | Sensor 5 V supply | Regulated 5 V |

The repair manual's ECU terminal checks show the phase terminals at 9 to 14 V when powered, the Hall/sensor supply and Hall outputs around 4 to 5.5 V, and DTC C2307 when parking lock motor current reaches 50 A or more for the specified fault condition. Use a much lower bench current limit at first.

## Junkyard Parts To Pull

Since your actuator is still mounted and you do not have a pigtail, the best first purchase is not electronics. It is wiring.

Pull these from a 2004-2009 Prius if you can:

- The S1 shift control actuator pigtail with as much wire length as possible. The 2004 wiring manual lists S1 as connector `90980-12446`.
- The transmission control ECU connectors/pigtails, especially if the car still has the matching harness section. These give you Toyota wire colors and a reference harness to probe.
- The P CON MTR fuse holder / relay area or at least a matching fused feed section. Toyota protected the motor feed through the P CON MTR fuse.
- Brake switch connector and a bit of harness if you want an OEM-style brake input.
- Any nearby bracketry or covers around the actuator, because they can be useful for mounting an external position sensor.

Do not cut the actuator connector right at the plug. Leave enough wire to stagger splices and strain-relieve the harness.

## Hardware Configuration To Buy Or Build

Recommended bench setup:

- Controller: Arduino Uno Rev3, Uno R4 Minima, or Nano Every. A 5 V Arduino is convenient because the Toyota Hall sensor supply/signals are 5 V-class. Arduino's Uno Rev3 specs list 5 V operation and 6 PWM-capable I/O; Uno R4 Minima is also a 5 V board.
- Connector: salvage the S1 actuator pigtail if possible. The 2004 wiring manual lists S1 shift control actuator connector part number `90980-12446`, but a used harness pigtail is usually the easiest path.
- Power supply: start with a current-limited 12 V bench supply at 2 to 5 A. Move upward only after the actuator moves correctly. Add an inline ATC/ATO fuse holder with 5 A, 10 A, 15 A, and 30 A fuses available. OEM uses a 30 A P CON MTR fuse, but that is not where bench testing should start.
- Motor power enable: one relay or smart high-side switch feeding S1-2 MGNA. A robust option class is an automotive high-side switch such as Infineon Power PROFET. For example, BTS50015-1TAD is a protected high-side switch family member with diagnostic/protection features and a 39 A listed load current on Infineon's page. A plain relay can work for bench testing if it is fused and has a flyback diode on its coil.
- Phase drive: three low-side N-channel MOSFET channels, one each for MUA/MVA/MWA. Choose logic-level MOSFETs with generous voltage/current margin, heat sinking, and low Rds(on). Add a MOSFET gate driver rather than driving large FET gates directly; Microchip TC4427 is a dual 1.5 A MOSFET gate driver, so two chips cover three channels.
- Inductive protection: put a flyback/clamp path on each phase. For a low-side switched coil, use a diode or TVS clamp from the phase/MOSFET drain node back to the +12 V motor rail. With a diode, the anode goes to the phase/drain node and the cathode goes to +12 V. Add a TVS across +12 V to ground and bulk capacitance near the driver.
- Current sensing: put a Hall current sensor in the +12 V feed to MGNA. Allegro ACS758 supports 3.0 to 5.5 V operation, galvanic isolation, 120 kHz typical bandwidth, and AC/DC proportional output. A 50 A ACS758/ACS724 module is a reasonable bench choice. The sketch defaults to a 40 mV/A, zero-at-Vcc/2 sensor style.
- Safety inputs: a latching E-stop that removes 12 V motor power, a separate ARM switch to the Arduino, a brake/safe input for unlock, and two independent lock/unlock limit switches or proximity sensors. I would not trust Hall edge counting alone until you have characterized this exact actuator under load.
- Wiring: use 16-18 AWG for motor/common wiring during higher-current tests, 22 AWG for Hall/sensor lines, common logic/power ground, and strain relief. Keep phase wiring short.

For a real car project, I would add these requirements:

- Do not use an Arduino Uno as the only safety authority. It is fine for development, but the final controller should be on a rugged PCB or automotive/industrial controller with watchdog, brownout handling, reverse-battery protection, load-dump/transient protection, sealed connectors, and conformal coating or an enclosure.
- The motor supply should have two ways to shut off: a hard E-stop/main enable that removes +12 V from MGNA, and software-controlled phase outputs that default off.
- Park and unpark must require a verified zero-speed signal. Best practice is two independent zero-speed confirmations, such as a CAN-derived speed-ok signal plus a wheel/shaft-speed comparator relay.
- Unpark should require brake active. I would also require brake active for park unless your higher-level vehicle controller has a very clear reason not to.
- Add independent PARK and NOT-P confirmation. The Toyota Hall outputs show rotation/progress, but by themselves they are not a trustworthy absolute position indicator after power loss. Physical limit switches, sealed proximity sensors, or a redundant external shaft-position sensor are strongly preferred.
- Keep a mechanical parking brake. The parking pawl controller should not be the only device keeping the car still.

## 1996 Civic Swap Integration

Your current target is a Prius transaxle in a 1996 Honda Civic DX hatchback. In that setup, the Civic shifter/interlock should be treated as driver intent, not as proof that the Prius pawl actually moved.

Recommended signal split:

- Civic shifter/interlock: tells the controller whether the driver selected PARK or not-PARK.
- Brake switch: required before any pawl motion.
- Zero-speed signal: required before any pawl motion. The 1996 Civic VSS signal is a 5 V-class pulse line, but the final controller should condition it through an optocoupler, Schmitt trigger, or external zero-speed module rather than connecting raw vehicle wiring straight to the Arduino.
- Prius Hall signals: prove the actuator is making rotational progress while moving.
- Independent PARK/NOT-P sensors: prove the final actuator/pawl state.
- Civic ebrake: keep it as a mechanical backup. The sketch also reads it on A5, and you can set `REQUIRE_EBRAKE_FOR_PARK` or `REQUIRE_EBRAKE_FOR_UNPARK` true during early validation if you want the controller to refuse motion unless the parking brake is set.

All Civic-side signals in the sketch are assumed to arrive at the Arduino as clean active-low logic contacts. Do not feed 12 V brake/interlock wiring directly into Arduino pins. Use small automotive relays, optocouplers, or protected input circuits with series resistance, clamp protection, and a shared/known ground strategy.

For the Civic shifter inputs, avoid using one single park-pin wire where "wire disconnected" means "not park." Use two explicit signals if possible:

- `CIVIC_PARK_SELECTED`: active only when the shifter is in PARK.
- `CIVIC_NOT_P_SELECTED`: active only when the shifter is out of PARK.

The vehicle sketch faults if both are active or both are inactive. If the stock Civic shifter only gives you one useful park contact, add a small sealed relay, microswitch, or position sensor so the controller has two independent logic states.

## Custom CAN Bus

You mentioned a custom CAN bus in the project. That is a good fit for VCU/dash/status coordination, but the 1996 Civic ECU should not be expected to join that bus directly. Treat the Civic ECU and HVAC/interlock wiring as legacy hardwired I/O, then use a CAN gateway node to publish the useful states onto your custom bus.

For pawl control, CAN can provide supervisory context such as speed, drive-enabled state, shifter state, or VCU alive. It should not replace local hardwired safety inputs. If CAN says speed-zero but the hardwired zero-speed input disagrees, the pawl controller should fault and remove actuator motor power.

See `CAN_INTEGRATION_NOTES.md` for the longer architecture and draft message list.

Useful product reference pages:

- Arduino Uno Rev3 specs: https://store-usa.arduino.cc/products/arduino-uno-rev3
- Arduino Uno R4 Minima specs: https://docs.arduino.cc/hardware/uno-r4-minima/
- Allegro ACS758 current sensor: https://www.allegromicro.com/en/products/sense/current-sensor-ics/integrated-current-sensors/acs758
- Microchip TC4427 MOSFET gate driver: https://www.microchip.com/en-us/product/TC4427
- Infineon BTS50015-1TAD protected high-side switch: https://www.infineon.com/cms/en/product/power/smart-power-switches/high-side-switches/power-profet-plus-12v-automotive-smart-high-side-switch/bts50015-1tad/

## Sketches

There are three sketches:

- `PriusParkPawlBenchController.ino`: serial-command bench jogger for finding direction, current, Hall behavior, and travel.
- `PriusParkPawlVehicleController.ino`: hardwired-input vehicle controller draft with ARM, brake, ebrake sense, dual zero-speed inputs, Civic shifter PARK/not-PARK inputs, current trip, Hall-progress timeout, and independent lock/unlock confirmation inputs.
- `CivicAutoTransSignalEmulator.ino`: Arduino/STM32-style Civic automatic-transmission signal emulator. It receives MPH from CAN, generates VSS/mainshaft/countershaft pulse outputs, and drives protected range-switch pull-down outputs for a non-HX 1996 Civic automatic PCM/gauge harness. See `CIVIC_AT_ECU_EMULATOR.md`.

Start with the bench sketch, even though the actuator is mounted. Use it with the car safely lifted/chocked and the motor feed current-limited. Only move to the vehicle sketch after you have verified phase order, current, direction, and position confirmation.

## Bench Sketch Wiring Defaults

The included sketch is `PriusParkPawlBenchController.ino`.

| Arduino pin | Purpose |
| --- | --- |
| D9 | U phase MOSFET/gate-driver input, S1-7 MUA |
| D10 | V phase MOSFET/gate-driver input, S1-6 MVA |
| D11 | W phase MOSFET/gate-driver input, S1-1 MWA |
| D2 | RA Hall input, S1-5 |
| D3 | RB Hall input, S1-3 |
| D4 | RZ1 Hall input, S1-4 |
| D5 | Lock limit switch, active low |
| D6 | Unlock limit switch, active low |
| D7 | ARM switch, active low |
| D8 | Brake/safe input, active low |
| D12 | Motor power relay/high-side enable |
| A0 | Current sensor analog output |
| 5 V | RVC sensor supply, S1-9 |
| GND | RE2 sensor ground, S1-8, and driver logic ground |

The default sketch refuses motion unless ARM is active. Unlock also requires the brake/safe input. Limit switch inputs are enabled by default. Commands are sent over Serial at 115200 baud.

Serial commands:

| Command | Action |
| --- | --- |
| `s` | Print status, Hall bits, current, interlocks |
| `+` | Jog one commutation step positive |
| `-` | Jog one commutation step negative |
| `P` | Run toward park/lock |
| `N` | Run toward not-P/unlock |
| `u` / `d` | Raise/lower PWM duty |
| `z` | Zero the current sensor with motor off |
| `c` | Clear fault |
| `x` | Stop active motion and fault |
| `h` | Help |

## Vehicle Sketch Wiring Defaults

`PriusParkPawlVehicleController.ino` does not use serial commands for motion. Motion comes from hardwired request inputs.

| Arduino pin | Purpose |
| --- | --- |
| D9 | U phase MOSFET/gate-driver input, S1-7 MUA |
| D10 | V phase MOSFET/gate-driver input, S1-6 MVA |
| D11 | W phase MOSFET/gate-driver input, S1-1 MWA |
| D2 | RA Hall input, S1-5 |
| D3 | RB Hall input, S1-3 |
| D4 | RZ1 Hall input, S1-4 |
| D5 | Independent PARK/lock confirmation, active low |
| D6 | Independent NOT-P/unlock confirmation, active low |
| D7 | ARM/ignition enable, active low |
| D8 | Brake-ok input, active low |
| D12 | Motor power relay/high-side enable |
| A0 | Current sensor analog output |
| A1 | Zero-speed input 1, active low |
| A2 | Zero-speed input 2, active low |
| A3 | Civic PARK selected input, active low |
| A4 | Civic NOT-P selected input, active low |
| A5 | Civic ebrake/parking-brake switch, active low |

The vehicle sketch refuses movement unless ARM is enabled, brake is active, both zero-speed inputs are active, and exactly one Civic shifter input is active. It faults if speed-zero, brake, or valid shifter state is lost during a move. Faults are cleared by power-cycling or sending `r` over Serial during service.

## Bring-Up Sequence

1. With the actuator disconnected, test the MOSFET driver using three 12 V lamps or power resistors instead of the motor coils. Confirm only one phase output switches at a time.
2. Power the Hall sensor from regulated 5 V on RVC/RE2. Turn the actuator shaft by hand if possible and confirm `s` shows changing Hall bits.
3. Set the 12 V supply current limit low, install a 5 A fuse, and connect the actuator motor. Use `z`, then `s`.
4. Hold ARM active and use `+` / `-` jogs. Watch current, Hall edge changes, sound, and temperature. If the shaft moves opposite of your intended PARK direction, edit `PARK_DIRECTION` and `UNLOCK_DIRECTION` in the sketch.
5. Install/test lock and unlock limit switches before using `P` or `N` repeatedly. Test the limit switch state in `s` before the motor is connected.
6. Increase current limit only enough to move the mechanism reliably. Record Hall edge counts between lock and unlock, but keep the physical limit switches as the authoritative stop.

## Open Questions For Your Build

- Can you add independent lock/unlock limit switches or a visible external position sensor?
- Is your Civic shifter the original automatic shifter assembly with the A/T gear position and park-pin switches still intact?
- Will the car keep the Civic instrument cluster/VSS wiring, or is the speed signal coming from another controller?
- What is your main traction/control system, and can it provide a redundant zero-speed or drive-disabled signal?

Those answers determine whether the next version should stay a bench jogger or grow into a more formal controller with vehicle-speed, brake, and park-brake interlocks.
