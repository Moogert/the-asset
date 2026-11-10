# Honda-mediated EV fault indication

See the [simulator installation guide](ENGINE_SIMULATOR_INSTALL.md) for current
project connectors, wiring steps and output-profile behavior. Channels 6/7 remain
unfitted in the supplied board profile and acknowledge their categories as unavailable.

29 October 2026. **Software implemented; Honda ECU and electrical acceptance pending.**

The original Honda ECU generates OBD-II codes and drives the original check-engine
light. The Mega deliberately changes selected emulated sensor conditions in response
to EV fault categories. The Teensy records actual EV faults and coordinates those
conditions; detailed EV information is exported through USB, with a Bluetooth
transport interface provided but no module selected.

There is no reader-facing CAN endpoint, Honda diagnostic polling master or combined
MIL driver in the default Teensy image. Connect a compatible reader to the original
Honda socket using its verified native protocol. ANCEL AD310 remains an untested
candidate; CAN capability alone does not establish compatibility with this ECU.

## Fault indication

Eight project categories cover thermal, circulation, feedback, channel/speed,
battery/permission, communication, timing and configuration faults. All simultaneous
EV faults remain in the detailed record. The full category mask is applied together;
firmware does not rotate faults to manufacture a sequence of reader codes.

Mapping entries identify a conditioned sensor circuit, stimulus, verified electrical
interface, observed Honda code, measured detection delay and operating prerequisites.
All supplied vehicle mappings remain unverified. Synthetic channels 0–7 OPEN are
bench examples, not eight identified Honda circuits or guaranteed distinct codes.

The compile-time mapping is in `Vehicleprofile::indication_profile()`. The editable
[verification worksheet](../../configs/honda-fault-indication.json) records evidence
and is not loaded by firmware. Record the complete 37820- ECU number first.

Production application requires each selected entry's electrical and ECU verification,
nonzero observed code/delay and prerequisites. Unverified categories are acknowledged
as unavailable and cannot inject their configured fault. A synthetic profile requires
the physically isolated bench contact. Engine output still requires explicit arming,
a valid waveform profile, enable contact and watchdog supervision.

Applied status means the emulator accepted its configured stimulus, not that a scope
verified it or Honda reported a code. Record these as three separate acceptance results.
Open/low/high describe protected conditioning behavior; never short an ECU circuit
or use a GPIO directly on an automotive sensor/output wire.

## Communication and recovery

- Teensy sends the complete category mask every 100 ms over the existing translated
  UART. Version, mapping revision, 16-bit sequence and CRC-16 protect the transaction.
- Mega returns requested/applied/unavailable masks with sequence, revision and CRC.
  Duplicate/out-of-order commands cannot refresh its link heartbeat. Sequence wrap
  is supported; a disarmed emulator with an expired link may accept a restarted sequence.
- Loss of category communication for more than 500 ms stops the emulator and disables
  its output gate. An engine PING cannot bypass this category timeout once established.
  Reset starts disarmed; fresh commands never rearm it automatically.
- Removing an EV fault removes that category's stimulus; other active categories stay
  applied. Honda controls its own pending/stored code and MIL recovery rules.
- Native reader clearing is a Honda service operation. USB CLEAR removes inactive EV
  history only with verified parked/disarmed/zero-speed evidence. Neither operation
  resets traction faults or sends a traction enable command.
- A missing or failed emulator may not produce a prompt Honda warning. Independent
  hardware failure indication remains required and electrically unverified. Traction
  shutdown must never wait for Honda monitoring, USB or Bluetooth.

## USB and Bluetooth

DUMP emits bounded JSON Lines records with version, evidence classification, internal
fault identity/source, active/stored state, first/latest boot-relative time, occurrence
count, available context, mapping state and emulator health. Internal P1F labels remain
project record identifiers; they are not codes injected into Honda OBD.

Component-detail records additionally identify component, severity, value, limit and
sender clock. Optional CAN triplets 0x660–0x662 carry these details independently of
unchanged mask telemetry. Missing detail measurements stay invalid. Component details
are RAM-only; coarse EV fault history retains the existing journal. Export detailed
records before power removal. No cross-device clock synchronization is implied.

USB queues at most one record and sends bounded chunks. A future Bluetooth adapter
uses the same bytes through Diagnostictransport. Bluetooth is unbound: no radio,
UART pins, pairing, live wireless connection or physical data parity is claimed.
Host tests compare USB/Bluetooth mock outputs byte-for-byte and exercise backpressure.

## Hardware and budget

Retain Mega2560, Teensy4.1, DACs, protected buffers/disconnects, resistive emulators,
position drivers, characterized actuator loads, inter-board voltage translation,
automotive power protection and independent watchdog gates. The default gateway
needs one EV CAN interface; no additional reader CAN, K-line master or MIL-OR stage
is required. The original diagnostic wiring and lamp remain connected to Honda.

The earlier $622–$1,425 parts allowance remains a conservative planning reserve,
not an updated purchasing quote; remove obsolete interfaces and re-quote the exact
stimulus circuits before ordering. Bluetooth module/interface cost is unpriced until
selected. Existing BOM/evidence snapshots describe the earlier review and are historical.

## Acceptance

1. Run host waveform, category, CRC/sequence, timeout, multi-fault and record tests.
2. Measure stimulus amplitude, impedance, timing and reset/open behavior on a fixture.
3. Establish a healthy-emulation baseline on the identified ECU; disclose unrelated codes.
4. For every category, record pending/stored code, MIL, prerequisites, detection delay,
   recovery and repeated results. Test combinations, not only isolated faults.
5. Verify the named reader on the original socket; print the observed mapping dictionary.
6. Demonstrate USB records and, after module selection, Bluetooth parity on actual hardware.

A fault caused by the emulator or a genuine Honda circuit can yield the same code.
Reader text keeps its Honda meaning; use the project dictionary and detailed EV record
for interpretation. No emissions-readiness, complete engine-monitor emulation or
installed-vehicle approval is established by generated signals alone.

See [controller release](CONTROLLER_RELEASE.md), [harness](HONDA_HARNESS.md) and
[code dictionary](HONDA_DTC_CODES.md).
