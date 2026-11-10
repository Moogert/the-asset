"""Export synthetic controller replay data and presentation figures; never access hardware."""

import argparse
import csv
import hashlib
import io
import json
from pathlib import Path
import subprocess

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    result = subprocess.run(
        [str(args.executable.resolve())], check=True, capture_output=True, text=True
    )
    (args.output / "samples.csv").write_text(result.stdout, encoding="utf-8")
    rows = list(csv.DictReader(io.StringIO(result.stdout)))
    titles = ["MG1 heating", "Shared oil heating", "Raw temperature spike", "Opposing DC power"]
    fig, axes = plt.subplots(2, 2, figsize=(12, 8), constrained_layout=True)
    summary = []
    for index, ax in enumerate(axes.flat):
        run = [row for row in rows if row["run_id"] == f"revision_{index}"]
        time = [float(row["time_s"]) for row in run]
        if index < 3:
            ax.plot(time, [float(row["raw_temp_c"]) for row in run], label="Raw sensor")
            ax.plot(time, [float(row["filtered_temp_c"]) for row in run], label="Filtered sensor")
            ax.axhline(90, color="red", linestyle=":", label="Demo stop: 90 °C")
            ax.set_ylabel("Temperature [°C]")
            second = ax.twinx()
            second.plot(
                time,
                [float(row["mg1_factor"]) for row in run],
                "--",
                color="black",
                label="MG1 thermal factor",
            )
            second.plot(
                time,
                [float(row["mg2_factor"]) for row in run],
                ":",
                color="purple",
                label="MG2 thermal factor",
            )
            second.set_ylim(-0.05, 1.05)
            second.set_ylabel("Effort factor [0–1]")
            handles, labels = ax.get_legend_handles_labels()
            more, names = second.get_legend_handles_labels()
            ax.legend(handles + more, labels + names, fontsize=7, loc="lower left")
        else:
            ax.plot(time, [float(row["power1_w"]) for row in run], label="MG1 DC power")
            ax.plot(time, [float(row["power2_w"]) for row in run], label="MG2 DC power")
            ax.axvline(0.3, color="gray", linestyle=":", label="Opposition begins")
            ax.axvline(0.4, color="red", linestyle="--", label="100 ms confirmation")
            ax.set_ylabel("Signed DC power [W]")
            ax.legend(fontsize=8)
        events = [row for row in run if int(row["faults"]) != 0]
        summary.append(
            {
                "run_id": f"revision_{index}",
                "first_fault_s": float(events[0]["time_s"]) if events else None,
            }
        )
        ax.set_title(f"revision_{index}: {titles[index]}")
        ax.set_xlabel("Synthetic time [s]")
        ax.grid(alpha=0.25)
    fig.suptitle("SYNTHETIC DEMONSTRATION — teaching thresholds; no measured vehicle performance")
    fig.savefig(args.output / "controller-revision.svg")
    fig.savefig(args.output / "controller-revision.png", dpi=300)
    plt.close(fig)
    (args.output / "results.json").write_text(
        json.dumps(
            {
                "evidence": "synthetic_demo",
                "executable_sha256": hashlib.sha256(args.executable.read_bytes()).hexdigest(),
                "runs": summary,
            },
            indent=2,
        )
        + "\n",
        encoding="utf-8",
    )
    (args.output / "report.md").write_text(
        "# Controller revision replay\n\nEvidence: **synthetic demonstration**.\n\n"
        "![Controller replay](controller-revision.png)\n\n"
        "Settings: 10 ms samples; 100 ms temperature filter; 70/90 °C teaching derate/stop; "
        "5 °C shutdown recovery hysteresis; opposing power lower bound above 500 W for 100 ms.\n\n"
        "Local heating limits MG1; shared oil heating limits both. A raw spike triggers the hard limit "
        "before the filter reaches it. Sustained opposing power confirms after the declared interval. "
        "This replays monitoring only: production command shutdown remains latched in the release runtime. "
        "These plots establish neither physical inhibition latency nor component ratings.\n",
        encoding="utf-8",
    )
    print(args.output.resolve())


if __name__ == "__main__":
    main()
