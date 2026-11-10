# Offline bench testing and presentation tools

**Recorded-data tools only. No serial/CAN devices are opened, gains changed or motors commanded.**
The user reports a complete drivetrain and an MCU/basic instruments available;
their exact models, revisions, calibrations and connections remain unconfirmed.

## Start here

This workspace has an isolated `.venv-bench` environment. From the project root:

```powershell
.\.venv-bench\Scripts\python.exe tools/bench.py plan --out output/bench/my-plan
.\.venv-bench\Scripts\python.exe tools/bench.py demo --out output/bench/my-demo
```

Open `output/bench/my-demo/report/report.md`. Its seven SVG/300 dpi PNG figures
exercise before/after current response, disturbance recovery, charge/discharge
limiting, fault timing, thermal endurance and single/combined torque comparisons.
**All demo traces are analytic synthetic examples, not a validated Prius plant,
hardware-in-the-loop test or measured tuning result.** Demo gains/calibration
references must never be copied into real commissioning records.

Output directories must be new. Existing directories/files are not overwritten.
Create another named output folder to repeat an operation.

For a fresh checkout, use Python 3.12 and the saved dependency lock:

```powershell
python -m venv .venv-bench
.\.venv-bench\Scripts\python.exe -m pip install -r configs/bench-requirements.lock.txt
```

## Record a real experiment

1. Use the lab procedure in [BENCH_LAB.md](BENCH_LAB.md). Complete fixture and interface review first.
2. Copy the appropriate manifest and mapping from your plan's `templates/` folder. Six experiment types are supplied. Record unique run IDs, actual hardware/firmware, controller settings, conditions and event/windows. Unknown metadata starts as null.
3. Record the source clock, offset to the common reference and alignment uncertainty. Templates deliberately start unsynchronized. Do not assert synchronization without evidence.
4. Enter calibration expanded uncertainty, its coverage factor and certificate/method reference in each channel's own unit. Distinguish physical measurements, commands and estimates.
5. Map instrument column names and units explicitly, then import and analyze:

```powershell
.\.venv-bench\Scripts\python.exe tools/bench.py import `
  --csv recordings/current-step.csv `
  --mapping output/bench/my-plan/templates/current_step-mapping.json `
  --manifest output/bench/my-plan/templates/current_step-manifest.json `
  --out output/bench/current-run01
.\.venv-bench\Scripts\python.exe tools/bench.py analyze `
  --input output/bench/current-run01/manifest.json `
  --output output/bench/current-analysis.json
.\.venv-bench\Scripts\python.exe tools/bench.py report `
  --input output/bench/current-run01/manifest.json `
  --out output/bench/current-report
```

Import preserves the original CSV in `raw/`, copies the mapping and records hashes.
Later edits to the canonical capture, copied raw input or mapping are detected.
The original recording is never modified. Keep the complete run directory.
For several instruments or ordered capture chunks, repeat `--csv` and `--mapping`
in matching order. Declare every source/channel in the common manifest first.
The importer keeps a separate raw/mapping copy for each file and combines captures
under one run. It does not infer offsets, synchronize clocks or reorder chunks.

## Minimal data contract

The normalized CSV columns are exactly:

```text
time_s,source,signal,value,unit,valid
```

Each signal has one declared source and canonical unit. Timestamps must increase
strictly per signal; duplicates/regressions are rejected rather than silently
sorted. `valid` is 0 or 1. Invalid samples may have a blank value and break plots;
NaN/infinity are rejected. Gaps above each channel's `max_gap_s` invalidate that
analysis window. No interpolation bridges a bad window or extrapolates coverage.

Mappings use `time_column`, `time_unit` (`s`, `ms`, `us`), `source` and a `channels`
object. Each raw column declares `signal`, `unit`, optional `scale`/`offset`, and
optional `valid_column` (0/1). Conversion order is `(raw * scale + offset)` in the
declared input unit, followed by unit conversion. For ADC counts, the explicit
scale/offset must already express the declared physical unit; no sensor gain is
inferred. Supported conversions include mA/A, mV/V, rad/s/rpm and kW/W.

Run manifests use schema version 1, one evidence class (`measured`, `simulated`,
`synthetic_demo`), captures, sources, channels and experiments. The generated
templates define all required fields. Optional measurement bandwidth/filtering
notes may be added to channel metadata and are retained in the report JSON.

`source.offset_s` is **added** to source-local timestamps to reach the common
reference. `alignment_uncertainty_s` is a declared bound; sample quantization is
also included in fault timing. Event and analysis windows use that common clock.

## Campaigns and uncertainty

A campaign JSON contains `schema_version: 1` and `runs`, a list of run-manifest
paths relative to the campaign. Use the generated campaign template. Mixed
evidence classes, duplicate run IDs and reused captures across measured runs are
rejected. Separate acquisitions and a stable procedure are still needed to
justify statistical independence.

For added torque, assign matching `pair_id` values to MG2-only and combined runs;
use `mode` values `mg2_only`/`combined`. MG1-only runs are reported separately.
Each pair must match firmware/settings/fixture, speed within 1%, actual MG2 command
within 0.1 percentage point, voltage within 2% and MG2 temperature within 5 C.
The latter tolerances are study defaults, adjustable in `comparison_tolerances`.
The steady window must also hold speed within 2% peak-to-peak, MG2 command within
0.1 percentage point, voltage within a 2% span and MG2 temperature within a 5 C
span. Voltage/temperature span criteria can be declared in the torque experiment.
Shared pack limiting must not reduce MG2 effort.

Target five independent matched pairs. The analysis computes the Student-t 95%
repeatability interval from **paired run means**. It adds the largest pair's summed
declared torque calibration bounds to the repeatability margin for a conservative
comparison bound. Calibration uncertainty is never divided by sample count.
This sum is not presented as one calibrated 95% interval. Unknown correlation is
bounded conservatively; no automatic certification or significance claim is made.

Separate channel DC current measurements/validated estimates are needed to assess
circulating power; the initial 5 A threshold is an adjustable study screen, not a
motor rating. Efficiency requires synchronized calibrated physical torque, speed,
voltage and current plus a declared boundary. It uses mean products `mean(T*w)`
and `mean(V*I)`. This version does not propagate efficiency uncertainty.

## Interpreting results

| Result | Meaning |
| --- | --- |
| resolved | This metric's declared data checks passed; not hardware acceptance |
| partial | Some metrics are available; read the listed limitations |
| unresolved | Necessary channels, timing, samples or other evidence are insufficient |
| measured_added_torque_supported | Numerical evidence flag requiring measured class, five matched pairs, complete traceability and increase beyond the conservative bound; lab review remains required |

Current rise needs ten samples across the 10-90% interval; excessive baseline
noise leaves dynamic metrics unresolved. Settling must persist for the declared
hold interval. Fault timing requires physical scope/DAQ trigger and inhibit
channels; CAN/software status flags cannot establish physical shutdown latency.
Thermal stabilization uses a declared slope, final-window span (default 1 C) and
minimum duration; a near-zero trend with large temperature cycling is not stable. It does not
establish magnet hotspots, safe component limits or continuous power ratings.

Successful CLI execution returns 0 even when metrics are unresolved; inspect JSON
statuses. Invalid inputs return 2. Figures always retain evidence classification,
units, run/settings references and visible non-measured labeling where applicable.

Outputs include `report.md`, `analysis.json`, `summary.csv`, `run-index.csv`,
`calibration.md`, `environment.json` and `figures/`. The environment records exact
package versions and analysis source hashes. Use SVGs for slide scaling and PNGs
for straightforward insertion. Keep unfavorable runs and disclose exclusions.

## Verification

```powershell
.\.venv-bench\Scripts\python.exe -m unittest discover -s tests -p test_bench.py -v
cmake -S . -B build-capstone -G "Visual Studio 17 2022" -DBUILD_TESTING=ON
cmake --build build-capstone --config Debug
ctest --test-dir build-capstone -C Debug --output-on-failure
```

CMake discovers this workspace's bench environment. Elsewhere, set
`-DBENCH_PYTHON=/absolute/path/to/python` to enable `bench_analysis_regression`.
The C++ source scope and MCU interfaces are unchanged by these offline tools.
