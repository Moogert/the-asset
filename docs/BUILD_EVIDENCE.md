# Compile evidence — 10 November 2026

| Target | Result | Flash | Static RAM |
|---|---|---:|---:|
| Bench controller / Uno | PASS | 7,896 / 32,256 bytes | 395 / 2,048 bytes |
| Vehicle draft / Uno | PASS | 7,156 / 32,256 bytes | 638 / 2,048 bytes |

Tool versions: PlatformIO Core 6.1.18, Atmel AVR platform 5.1.0,
Arduino AVR framework package 5.2.0 and AVR toolchain package 1.70300.191015
(GCC 7.3.0). Commands and configuration are in the root README and each project.

Both builds use `ENABLE_ACTUATOR_OUTPUTS = false`. The inherited unused bench
parameter is explicitly documented; the project-source warning was resolved.
The initial full builds reported unused-parameter warnings in Arduino's upstream
`new.cpp`; no dependency source was changed.

These are compile/size results only. There was no flashing, actuator motion,
oscilloscope capture, loaded test or vehicle acceptance. Stack margin, runtime
timing, protection coverage and enabled-output behavior remain unmeasured.
SHA-256 hashes in `source-manifest.json` identify the published files.
