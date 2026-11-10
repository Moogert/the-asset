> Historical architecture, superseded 29 October 2026. Do not use as the vehicle wiring specification.

# Honda emulator interface and harness schedule

**Concept/interface specification, 28 October 2026. No Honda cavity is released.**
The MCU pins below describe the delivered firmware, not direct vehicle connections.
Every Honda connector/cavity, sensor curve and load rating remains pending the
actual ECU label and matching factory electrical/service information.

## Signal schedule

| Circuit | Direction | MCU assignment | Required external interface / verification |
| --- | --- | --- | --- |
| Required position pickups: candidate CKP/TDC/CYP and any additional crank/misfire pickup | Simulator â†’ ECU | Mega D22/D23/D24; D25 reserved fourth channel | Verified per-channel pattern and sensor electrical type; protected single-ended or differential/bipolar driver as required. Default three-channel pattern is synthetic. |
| Vehicle speed | Simulator â†’ ECU/cluster | Mega D26 | Protected driver matching actual VSS circuit and measured scaling; installed frequency derives from fresh actual EV speed |
| TPS / MAP | Simulator â†’ ECU | DAC channels 0/1 | Buffered, limited outputs referenced to verified sensor ground; verify voltage range, source impedance and ECU reference interaction |
| Upstream/downstream oxygen signals, if fitted | Simulator â†’ ECU | DAC channels 2/3 | Correct low-level voltage/current interface and impedance; actual sensor type must be identified |
| ECT / IAT | Simulator â†’ ECU | D42â€“44 / D45â€“47 select two external 8-position resistance ladders | Characterized resistors/switches matching actual curves; selectors do not drive ECU pins directly |
| Spare analog functions | Simulator â†’ ECU | DAC channels 4â€“7 | Unassigned, not permission to connect a guessed sensor |
| Analog disconnects | Simulator â†’ interface | Mega D34â€“41, channels 0â€“7 | HIGH connects, LOW opens externally. Channels 4/5 can gate ECT/IAT ladders; do not connect their spare DAC voltages to thermistor circuits |
| Injector observation | ECU â†’ simulator | Mega D2 interrupt | Protected, correctly polarized load-current/driver-state observation; no direct connection to an injector power wire |
| Oxygen heater observation | ECU â†’ simulator | Mega D3 | Protected logic indicating actual heater operation; heater dummy load remains a separate rated circuit |
| Position/analog global gate | Simulator â†’ interface | Mega D8 | Active-high enable, external pulldown and independent watchdog override; all output stages open/off on loss |
| Enable / bench contacts | Switches â†’ Mega | D9 / D10 | Active-low, debounced/conditioned as needed; bench jumper physically separate from vehicle harness |
| External watchdog heartbeat | Mega â†’ watchdog | D11 | 100 ms toggles; hardware timeout and gate response must be selected and measured |
| DAC SPI | Mega â†’ four MCP4922-class DACs | MOSI D51, SCK D52, CS D30â€“33 | Four dual-channel DACs; gain/reference/buffering must match the 0â€“5 V synthetic conversion or be recalibrated |
| Inter-board status/control | Both directions | Mega Serial1 TX18/RX19 â†” Teensy Serial2 RX7/TX8 | Protected 5 Vâ†”3.3 V translation; no direct 5 V into Teensy; suitable automotive routing or differential extension |
| EV telemetry CAN | Supervisor â†’ gateway | Teensy CAN2 TX1/RX0 | External transceiver; native project triplet protocol, not guessed OpenInverter/Toyota frames |
| Reader CAN | Reader â†” gateway | Teensy CAN1 TX22/RX23 | Separate transceiver and correctly terminated diagnostic bus; no raw forwarding to traction network |
| Honda diagnostics | Honda ECU â†” gateway | Teensy Serial3 TX14/RX15; D17 optional L-init driver command | Protected K-line physical layer and 3.3 V-safe logic; exact protocol/init/header profile pending |
| Honda MIL observation | Honda ECU â†’ gateway | Teensy D6 | Conditioned active-low logic; do not wire the lamp's 12 V circuit to GPIO |
| Added warning request | Gateway â†’ lamp interface | Teensy D9 | Protected current-rated stage, verified electrical OR with Honda request; GPIO HIGH requests warning |
| External gateway watchdog | Teensy â†’ watchdog | D10 | 100 ms toggles; independent failure warning must work with gateway power removed |
| Parked / armed evidence | Vehicle â†’ gateway | Teensy D4 / D5 | Conditioned physical inputs; LOW means parked, LOW means armed. Production clear also requires fresh telemetry and valid zero speed |
| Diagnostic bench override | Switch â†’ gateway | Teensy D12 | Ground only on isolated fixture; remove before installation |

All rows need calibration evidence or a documented digital threshold. Record
Honda connector name/cavity, wire color, drawing page, measured values, interface
revision and sign-off in [the editable worksheet](../../../configs/honda-ecu-interface.json).
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

## Diagnostic connector and lamp concept

The new reader-facing connector is the **CAN endpoint only**: SAE J1962 positions
6/14 are CAN-H/L, 4/5 grounds, 16 protected battery supply. Verify connector face
orientation, ratings and the actual harness; do not mirror a wire-side drawing.
Keep a separately labeled original Honda service connector. Do not simply add
the original diagnostic line to the new reader connector: a reader might select
Honda's native protocol and bypass the combined gateway.

Use appropriate termination at both ends of each CAN segment, accounting for
what the reader/fixture already contains. Measure the network before powering it.
Diagnostic supply is separately fused; no traction HV enters either connector.

Lamp concept: retain the Honda output's electrical function; add a verified,
isolated/protected request path that can assert a warning without back-driving
the ECU. Provide a separate hardware failure indication for loss of gateway or
emulator power/heartbeat. The driver polarity, diagnostic sensing and current
rating depend on the actual cluster circuit and remain unreleased.

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
   and direct/gateway diagnostic comparisons pass. Archive scope captures and codes.
