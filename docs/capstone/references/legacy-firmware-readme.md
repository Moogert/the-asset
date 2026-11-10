> Historical combined-OBD architecture. Superseded 29 October 2026; not vehicle instructions.

# Civic engine emulator and diagnostic gateway

> **29 October controller revision:** see [current controller and topology](../CONTROLLER_RELEASE.md). The default Teensy now coordinates Honda-induced faults and USB details; Honda owns OBD/MIL. Earlier combined-gateway results describe the isolated legacy demonstration.


Review-stage Arduino firmware, 28 October 2026. **No board has been flashed or
connected by this work.** The supplied Honda profile is unverified and cannot arm
vehicle sensor outputs. The isolated CAN diagnostic endpoint is implemented.

Start with [the system guide](legacy-honda_ecu_and_obd.md),
[the interface schedule](legacy-honda_harness.md) and
[the editable identification/BOM worksheet](../../../configs/honda-ecu-interface.json).

## Build

From the repository root in PowerShell:

```powershell
python -m venv .venv-firmware
.\.venv-firmware\Scripts\python.exe -m pip install -r configs/firmware-requirements.lock.txt
.\.venv-firmware\Scripts\platformio.exe run -d firmware
cmake -S . -B build-capstone -G "Visual Studio 17 2022" -DBUILD_TESTING=ON
cmake --build build-capstone --config Debug
ctest --test-dir build-capstone -C Debug --output-on-failure
```

PlatformIO targets are `mega_engine` and `teensy_gateway`; both generate
`firmware/.pio/build/<target>/firmware.hex` and `.elf`. Platform and framework
versions are pinned in `platformio.ini`; the build evidence records installed
packages and binary hashes. **Do not use an upload command as part of a software
test.** Upload/commissioning is a separate physical procedure.

## Mega bench commands

USB and the gateway UART use 115200 baud, newline-terminated ASCII, maximum 95
characters per line. Commands reply `OK` or `ERR`; malformed/overflowing lines
are rejected. Hardware enable D9 and bench contact D10 must both be grounded
through their intended switches for a synthetic bench run.

| Command | Behavior |
| --- | --- |
| `ARM BENCH` | Enable synthetic profile only with bench contact; never a Honda profile |
| `ARM VEHICLE` | Rejected by the supplied unverified profile |
| `DISARM` | Stop signals and disable external stages |
| `PING` | Heartbeat; loss for over 500 ms disarms |
| `STATE STOP`, `STATE CRANK`, `STATE RUN`, `STATE WARM` | Select scenario; CRANK sets virtual RPM to 250 |
| `RPM 1000` | Set virtual RPM, bounded 200â€“8,000 in the synthetic profile |
| `THROTTLE 25`, `LOAD 40` | Synthetic percentage inputs, 0â€“100 |
| `FAULT 2 OPEN` | Analog channel 0â€“7: `OPEN`, `LOW`, `HIGH`, `NONE` |
| `DROP 1` | Drop position channels by bit mask; `DROP 0` restores |
| `VSS 100000` | Synthetic bench example: 100,000 mHz = 100 Hz; zero stops it |

Keep sending `PING` at least every 100 ms during isolated bench tests. The gateway
sends heartbeat automatically, but never automatically arms the engine emulator.
Commands that change mode/RPM restart the synthetic phase schedule; measure this
transition before selecting a real engine model. No smooth-acceleration Honda
misfire-monitor compatibility is claimed.

Timer1 schedules position edges; Timer3 schedules VSS. Do not use Servo/tone or
other libraries that reconfigure these timers. GPIO levels are interface-driver
commands, not connections to Honda sensor wires. A missed edge causes disarm
rather than a burst of late pulses. The 250 ms MCU watchdog and external gate
pulldowns supplement the required independent watchdog hardware.

Protected injector feedback on D2 and heater-state input on D3 influence the
teaching mixture model. The pulse-to-mixture relationship and resistance-ladder
indices are synthetic; they do not constitute a calibrated Honda engine/catalyst.

## Gateway demonstration

Connect only the isolated diagnostic fixture described in the interface guide.
The gateway exposes classic 11-bit CAN at 500 kbit/s. It responds to functional
`0x7DF` and physical `0x7E0` requests on `0x7E8`.

- `DUMP`: USB source/active/stored/pending/context records, emitted incrementally.
- `SIM 80 200020 1`: with Teensy D12 grounded, inject synthetic legacy/runtime
  hexadecimal fault masks and MG1 channel fault. Repeat after at least 500 ms
  to demonstrate pending-to-stored promotion. Missing real-source faults remain.
- `SIM 0 0 0`: end those synthetic assertions; confirmed history persists.

Ground the bench park switch D4 and leave arm input D5 high to demonstrate local
history clearing. **This bench override must be removed before installation.**
Diagnostic clearing never resets the traction supervisor, erases active assertions
or sends a clear request to the Honda ECU. Original Honda history is serviced
through its direct diagnostic connection and re-imported if still reported.

The current gateway offers MIL/DTC status and stored/pending code lists, not generic
live-data or freeze-frame PIDs. Detailed context is available over USB. No emissions
monitor completion or permanent-code support is advertised. A handheld reader's
compatibility remains an acceptance test, not a consequence of successful compilation.

## EV telemetry publisher contract

`Civic::diagnostic_snapshot` in `include/diagnostics/supervisor_bridge.h` accepts
the existing supervisor/runtime fault values plus explicitly sourced channel and
measurement context. `encode_telemetry` produces the triplet below. The bridge
does not transmit; the actual supervisor CAN driver remains an integration task.
Send complete triplets every 50 ms; the receiver assembles them within 20 ms and
declares the source stale after 250 ms. Fields are little-endian.

| CAN ID | Eight-byte payload |
| --- | --- |
| `0x650` | Version=1, counter, state flags, speed low/high (cm/s), context-valid mask, reserved=0, reserved=0 |
| `0x651` | Legacy C fault mask (4 bytes), C++ runtime fault mask (4 bytes) |
| `0x652` | Pack voltage low/high (0.1 V), temperature low/high (signed 0.1 Â°C), channel-fault mask, reserved=0, CRC low/high |

State flags: bit 0 parked, bit 1 armed; other bits zero. Context-valid bits are
speed=0, pack voltage=1, temperature=2. Channel-fault bits are MG1=0, MG2=1.
CRC is CRC-16/CCITT-FALSE over the first 22 concatenated payload bytes. Counters
advance modulo 256; duplicates/backwards jumps are rejected. IDs and bitrate are
project defaults, not OEM mappings. A publisher reset may require receiver
reinitialization or counter progression before freshness recovers. CRC/counters
detect transmission/replay mistakes; they are not source authentication.

Leave unmeasured context validity bits clear. Never infer MG1/MG2 identity from
an aggregate fault. Each supplied fault mask represents one declared supervisor
implementation; when both are published, duplicate meanings may produce distinct
project codes. The gateway is read-only with respect to this control network.

The serial Mega/gateway link is a protected local interconnect, not an authenticated
vehicle network. For longer routing, implement and validate a suitable differential
physical layer rather than extending unprotected TTL wiring through the car.

## Reproduce the presentation example

With the existing isolated bench-analysis environment installed:

```powershell
.\.venv-bench\Scripts\python.exe tools/civic_demo.py --executable build-capstone/Debug/civic_io_demo.exe --output output/civic-review-new
```

Use a new output directory for each export. The command writes a Markdown report,
raw JSON/CSV, SVG and 300 dpi PNG from the compiled portable core. It has no device
driver and never connects to an ECU. `output/civic-review/` is the reviewed example.
