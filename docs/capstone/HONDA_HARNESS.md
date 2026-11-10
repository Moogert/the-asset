# Honda emulator interface and harness schedule

Start with the [simulator installation guide](ENGINE_SIMULATOR_INSTALL.md) for
the project connector schedule, interposer steps and three Mermaid diagrams.

**Concept/interface specification, 29 October 2026. No Honda cavity is released.**
The MCU pins below describe the delivered firmware, not direct vehicle connections.
Every Honda connector/cavity, sensor curve and load rating remains pending the
actual ECU label and matching factory electrical/service information.

## Signal schedule

| Circuit | Direction | MCU assignment | Required external interface / verification |
| --- | --- | --- | --- |
| Required position pickups: candidate CKP/TDC/CYP and any additional crank/misfire pickup | Simulator → ECU | Mega D22/D23/D24; D25 reserved fourth channel | Verified per-channel pattern and sensor electrical type; protected single-ended or differential/bipolar driver as required. Default three-channel pattern is synthetic. |
| Vehicle speed | Simulator → ECU/cluster | Mega D26 | Protected driver matching actual VSS circuit and measured scaling; installed frequency derives from fresh actual EV speed |
| TPS / MAP | Simulator → ECU | DAC channels 0/1 | Buffered, limited outputs referenced to verified sensor ground; verify voltage range, source impedance and ECU reference interaction |
| Upstream/downstream oxygen signals, if fitted | Simulator → ECU | DAC channels 2/3 | Correct low-level voltage/current interface and impedance; actual sensor type must be identified |
| ECT / IAT | Simulator → ECU | D42–44 / D45–47 select two external 8-position resistance ladders | Characterized resistors/switches matching actual curves; selectors do not drive ECU pins directly |
| Spare analog functions | Simulator → ECU | DAC channels 4–7 | Unassigned, not permission to connect a guessed sensor |
| Analog disconnects | Simulator → interface | Mega D34–41, channels 0–7 | HIGH connects, LOW opens externally. Channels 4/5 gate ECT/IAT ladders; do not connect their spare DAC voltages to thermistor circuits |
| Injector observation | ECU → simulator | Mega D2 interrupt | Protected, correctly polarized load-current/driver-state observation; no direct connection to an injector power wire |
| Oxygen heater observation | ECU → simulator | Mega D3 | Protected logic indicating actual heater operation; heater dummy load remains a separate rated circuit |
| Position/analog global gate | Simulator → interface | Mega D8 | Active-high enable, external pulldown and independent watchdog override; all output stages open/off on loss |
| Enable / bench contacts | Switches → Mega | D9 / D10 | Active-low, debounced/conditioned as needed; bench jumper physically separate from vehicle harness |
| External watchdog heartbeat | Mega → watchdog | D11 | 100 ms toggles; hardware timeout and gate response must be selected and measured |
| DAC SPI | Mega → four MCP4922-class DACs | MOSI D51, SCK D52, CS D30–33 | Four dual-channel DACs; gain/reference/buffering must match the 0–5 V synthetic conversion or be recalibrated |
| Inter-board status/control | Both directions | Mega Serial1 TX18/RX19 ↔ Teensy Serial2 RX7/TX8 | Protected 5 V↔3.3 V translation; no direct 5 V into Teensy; suitable automotive routing or differential extension |
| EV telemetry CAN | Supervisor → gateway | Teensy CAN2 TX1/RX0 | External transceiver; native project triplet protocol, not guessed OpenInverter/Toyota frames |
| External gateway watchdog | Teensy → watchdog | D10 | 100 ms toggles; independent failure warning must work with gateway power removed |
| Parked / armed evidence | Vehicle → gateway | Teensy D4 / D5 | Conditioned physical inputs; LOW means parked, LOW means armed. Production clear also requires fresh telemetry and valid zero speed |
| Diagnostic bench override | Switch → gateway | Teensy D12 | Ground only on isolated fixture; remove before installation |

All rows need calibration evidence or a documented digital threshold. Record
Honda connector name/cavity, wire color, drawing page, measured values, interface
revision and sign-off in [the editable worksheet](../../configs/honda-ecu-interface.json).
Wire color and a generic OBD2A pin chart are insufficient release evidence.

## Additional ECU circuits to inventory

| Function | Assessment required |
| --- | --- |
| ECU supplies/grounds | Battery keep-alive, ignition, start request, power ground, sensor ground and 5 V references; protect power sequencing and prevent backfeed |
| Ignition/injectors | Driver type, monitored current, inductive behavior, dwell/pulse response and diagnostic loads; separate from sensor emulation |
| O2 heaters / idle-air / purge | Output polarity, current/PWM and thermal duty; plausible feedback where monitored |
| EVAP / fuel-tank pressure | Exact variant and diagnostic strategy; tank removal does not establish a constant-voltage replacement |
| Alternator / ELD / electrical load | Relationships to charging and ECU load calculations after engine removal |
| Fan / A/C / relay controls | Preserve necessary body functions; establish independent EV cooling/assist behavior |
| Brake/clutch/start switches | Verify manual-car interlocks and signal levels; do not defeat a start or motion interlock |
| Additional knock/position/pressure inputs | Determine fitted circuits from actual ECU; unused firmware channels are not confirmed substitutes |

The default firmware does not implement a validated EVAP, misfire, ignition-load
or catalyst model. Mark each applicable monitor as unverified until demonstrated.
If the identified ECU needs additional timed channels or waveform bandwidth,
revise and retest the hardware allocation before enabling its profile.

## Original diagnostic connector and lamp

Retain the original Honda diagnostic socket, native protocol wiring and ECU-controlled
MIL. The default Teensy image connects to neither the reader's diagnostic lines nor
the Honda MIL circuit. Do not add the legacy CAN endpoint or a second diagnostic master.
The separate `teensy_legacy_obd_demo` build is for an isolated laboratory fixture only.

Teensy sends CRC-checked category commands over Serial2 to Mega Serial1. Those commands
select already-reviewed conditioned stimuli; they do not assign new Honda cavities.
USB is the implemented detailed-data interface. Bluetooth pins/module remain unassigned.
Maintain protected power, grounds, diagnostic-supply fusing and connector orientation.
Provide independent watchdog-driven failure indication separately from Honda's MIL.

## Harness release checklist

1. Record ECU label, market/variant, connectors and factory diagram pages.
2. Verify each pin by de-energized continuity and appropriate powered measurements.
3. Characterize each interface on an isolated fixture, including open/short and
   reset conditions. Measure reference/ground offsets and loads under duty.
4. Use a removable breakout with labeled test points, branch fuses, strain relief
   and separation of sensor returns from load currents. Disable outputs by hardware.
5. Verify no unintended fuel/ignition actuation, no GPIO exposure to vehicle voltage,
   no 5 V applied to Teensy and no contention with connected real sensors.
6. Release the production profile only after waveform, calibration, load, warning
   and native-reader and emulator fault-indication tests pass. Archive scope captures and codes.
