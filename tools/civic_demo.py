"""Export actual portable-core demonstration output; never opens a serial or CAN device."""

import argparse
import csv
import hashlib
import json
from pathlib import Path
import subprocess


# Run the compiled portable core and export explicitly synthetic evidence; never connect a device.
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists():
        parser.error("Use a new output directory to preserve previous evidence")
    data = json.loads(subprocess.check_output([str(args.executable.resolve())], text=True))
    if data["classification"] != "synthetic_demo":
        parser.error("Demonstration evidence class missing")
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    args.output.mkdir(parents=True)
    data["executable_sha256"] = hashlib.sha256(args.executable.read_bytes()).hexdigest()
    (args.output / "results.json").write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")
    with (args.output / "position-edges.csv").open("w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        writer.writerow(["time_us", "position_mask", "classification"])
        writer.writerows((t, mask, "synthetic_demo") for t, mask in data["edges"])
    figure, axes = plt.subplots(3, 1, figsize=(10, 5), sharex=True)
    for bit, axis in enumerate(axes):
        axis.step(
            [r[0] / 1000 for r in data["edges"]],
            [(r[1] >> bit) & 1 for r in data["edges"]],
            where="post",
            color="#126782",
            linewidth=1.7,
        )
        axis.set_ylabel(["CKP logic", "TDC logic", "CYP logic"][bit])
        axis.set_ylim(-0.15, 1.2)
        axis.set_yticks([0, 1])
        axis.grid(alpha=0.2)
    axes[-1].set_xlabel("Scheduled time (ms); interface commands, not measured sensor voltage")
    figure.suptitle(
        "SYNTHETIC DEMONSTRATION • CIVIC-SYN-001 • virtual 1,000 rpm\nArtificial 24/4/1 pulses per 720°; no Honda pattern verification",
        fontsize=11,
    )
    figure.tight_layout()
    figure.savefig(args.output / "position-pattern.svg")
    figure.savefig(args.output / "position-pattern.png", dpi=300)
    plt.close(figure)
    codes = ", ".join(f["code"] for f in data["faults"])
    report = f"""# Civic firmware software demonstration

**Evidence: synthetic demonstration only. Run {data['run_id']}.**

The compiled portable core generated two artificial 720-degree cycles at
1,000 virtual rpm. This figure shows scheduled logic levels, not physical
voltage, jitter measurements or a verified Honda pattern.

![Synthetic position events](position-pattern.png)

The injected original-code and EV examples produce this combined stored-code
list: **{codes}**. The Honda-source record is deliberately injected; no Honda
ECU or OBD reader was connected. `results.json` retains the Mode 03 payload,
source identifiers and executable hash. `position-edges.csv` retains event times.

Physical acceptance still requires the exact ECU profile, measured interfaces,
scope captures, original ECU responses and the selected basic reader.
"""
    (args.output / "report.md").write_text(report, encoding="utf-8")
    print(args.output / "report.md")


if __name__ == "__main__":
    main()
