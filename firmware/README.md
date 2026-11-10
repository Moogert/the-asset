# Honda engine emulator and EV diagnostic coordinator

The [simulator installation guide](../docs/capstone/ENGINE_SIMULATOR_INSTALL.md)
contains the current connector netlist and Mermaid diagrams. Board pins are named
in `include/emulator_pins.h`; `emulator_io` validates output ranges and temperature
ladder fault selections. Serial parsing runs outside timer interrupt exclusion.

29 October 2026. **Review images only; exact ECU/hardware interfaces remain unverified.**

- `mega_engine`: timer-driven engine emulation and conditioned fault stimuli.
- `teensy_gateway`: EV telemetry/history, category coordination and USB records.
- `teensy_legacy_obd_demo`: explicitly isolated historical synthetic OBD experiment;
  excluded from default builds and never used on the vehicle diagnostic socket.

The original Honda ECU owns OBD-II codes and its MIL. Default Teensy firmware does
not transmit reader CAN, poll Honda diagnostics or drive the original lamp. Bluetooth
is an unbound transport interface; no radio or pins are selected.

## Build

```powershell
python -m venv .venv-firmware
.venv-firmware/Scripts/python.exe -m pip install -r configs/firmware-requirements.lock.txt
.venv-firmware/Scripts/python.exe -X utf8 -m platformio run -d firmware
```

Pinned platforms/packages remain in `platformio.ini`. The produced HEX/ELF files
have not been flashed. Compilation is not electrical or ECU acceptance.

## Vehicle release gates

`vehicle_profile.h` keeps Honda identification, engine model, EV telemetry and
service inputs unverified. The waveform is synthetic; production count/mask/RPM
remain zero. No Honda connector cavity is assigned. Set release fields only from
recorded factory references and measured interfaces.

The indication profile adds eight independently reviewed entries. Each production
entry needs electrical/ECU verification, observed code, detection delay and operating
prerequisites. Unknown entries report unavailable. The editable JSON worksheets are
not read by firmware. A fresh category handshake is required before ARM VEHICLE.
Neither category receipt nor return of healthy feedback automatically arms the engine.

See [harness schedule](../docs/capstone/HONDA_HARNESS.md) for MCU pins and external
conditioning. Mega Serial1 and Teensy Serial2 require protected 5 V/3.3 V translation.
Hardware watchdogs must disable outputs and provide independent failure indication.

## Serial interfaces

USB/UART use 115200 baud, newline-delimited text and bounded parsers. Existing Mega
bench commands remain: ARM BENCH, DISARM, PING, STATE, RPM, THROTTLE, LOAD, DROP,
FAULT and VSS. Synthetic arming needs the isolated bench contact and enable contact.
Vehicle waveforms still need validated actuator feedback and hardware gating.

Teensy USB supports DUMP, CLEAR and physically gated SIM. CLEAR removes inactive EV
history only when parked/disarmed evidence and fresh zero speed are verified. It
never clears Honda codes or traction inhibition. SIM remains synthetic and does not
hide missing physical-source diagnostics. USB record responses may be unavailable
while its bounded output queue is occupied; retry after the current dump completes.

Coordinator messages:

```text
CAT 1 <mapping_revision> <sequence> <category_mask> <crc16>
ACK 1 <mapping_revision> <sequence> <requested> <applied> <unavailable> <crc16>
```

Fields are unsigned decimal. CRC-16/CCITT-FALSE covers version, little-endian revision,
little-endian sequence and the mask bytes in displayed order. Command has six covered
bytes; acknowledgment eight. CRC has no authentication function. New sequences advance
modulo 65536, with forward distance below 32768. Duplicates cannot renew freshness.
Timeout after 500 ms stops the emulator; PING cannot override established CAT timeout.
A disarmed, expired receiver accepts a restarted sequence. Commands never rearm it.

DUMP emits JSON Lines v1: internal fault IDs, source, active/pending/stored state,
first/latest boot times, context validity/units, mapping revision/sequence/masks and
emulator health. Components add severity/value/limit and sender time. USB uses bounded
chunks; Bluetooth will use identical records through Diagnostictransport. The mock
parity test establishes software formatting only. Detailed records remain RAM-only;
coarse fault history retains the two-slot CRC journal. Export before power removal.

## Tests and presentation

Run CTest from the workspace root. The new controller_revision_regression tests
thermal/circulation/dispatch and category/detail transport behavior alongside existing
waveform/diagnostic tests. Legacy OBD tests remain explicitly laboratory tests.

[Controller release](../docs/capstone/CONTROLLER_RELEASE.md) includes topology, replay
commands and target-object compilation. [Honda guide](../docs/capstone/HONDA_ECU_AND_OBD.md)
and [mapping dictionary](../docs/capstone/HONDA_DTC_CODES.md) define physical acceptance.
No monitor readiness, observed Honda code, reader compatibility, physical latency or
full JSF compliance is established by these host tests.

## Parking-pawl projects

The independent [parking-pawl review projects](parking_pawl/README.md) use their
own PlatformIO configurations and are not part of the default Mega/Teensy build.
