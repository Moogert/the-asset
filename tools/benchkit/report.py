"""Deterministic slide-ready figures and an auditable Markdown report."""

import csv
import importlib.metadata
import json
from pathlib import Path
import sys
import textwrap

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

from . import VERSION
from .analysis import analyze
from .data import sha, writejson

PALETTE = ["#225B8A", "#D56A21", "#19755C", "#8A4F9B", "#6B7280", "#B63B42"]
plt.rcParams.update(
    {
        "font.family": "DejaVu Sans",
        "font.size": 10,
        "axes.spines.top": False,
        "axes.spines.right": False,
        "svg.hashsalt": "capstone-bench-v1",
        "axes.prop_cycle": plt.cycler(color=PALETTE),
    }
)


def classification(evidence):
    return {
        "measured": "MEASURED RECORDINGS - DECLARED CALIBRATION / REVIEW REQUIRED",
        "simulated": "SIMULATION - NOT MEASURED HARDWARE PERFORMANCE",
        "synthetic_demo": "SYNTHETIC DEMONSTRATION - NOT MEASURED",
    }[evidence]


# Export the same labeled figure to SVG and 300 dpi PNG with run/evidence identification.
def save_figure(fig, directory, name, title, runs, extra=""):
    evidence = runs[0].manifest["evidence_class"]
    fig.suptitle(title, x=0.10, ha="left", fontsize=15, fontweight="bold", color="#17354D")
    fig.text(0.10, 0.92, classification(evidence), fontsize=8, color="#A12F35", weight="bold")
    settings = "; ".join(
        f"{run.manifest['run_id']}: {json.dumps(run.manifest['controller_settings'], sort_keys=True)}"
        for run in runs
    )
    footer = textwrap.fill("Runs/settings: " + settings + (" | " + extra if extra else ""), 145)
    fig.text(0.10, 0.015, footer, fontsize=6.5, va="bottom", color="#465564")
    if evidence != "measured":
        fig.text(
            0.55,
            0.52,
            "NOT MEASURED",
            fontsize=34,
            color="#A12F35",
            alpha=0.09,
            rotation=18,
            ha="center",
            va="center",
        )
    fig.subplots_adjust(left=0.10, right=0.96, top=0.85, bottom=0.23, hspace=0.42)
    for ax in fig.axes:
        ax.grid(True, alpha=0.18)
    fig.savefig(
        directory / f"{name}.svg", metadata={"Date": None, "Creator": f"Capstone bench {VERSION}"}
    )
    fig.savefig(
        directory / f"{name}.png", dpi=300, metadata={"Software": f"Capstone bench {VERSION}"}
    )
    plt.close(fig)
    return {
        "name": name,
        "title": title,
        "svg": f"figures/{name}.svg",
        "png": f"figures/{name}.png",
    }


# Preserve missing/invalid gaps in plots instead of drawing an apparently continuous measurement.
def draw_trace(
    ax, run, signal, bounds, label=None, style="-", scale=1, value_factor=1, expected_unit=None
):
    if expected_unit and run.manifest["channels"][signal]["unit"] != expected_unit:
        ax.text(
            0.02,
            0.5,
            f"{signal}: incompatible unit; trace omitted",
            transform=ax.transAxes,
            color="#A12F35",
        )
        return
    trace = run.traces[signal]
    selected = (trace.time >= bounds[0]) & (trace.time <= bounds[1])
    # Explicit invalid samples break the drawn line. Large timestamp gaps do too.
    time = trace.time[selected]
    values = np.where(trace.valid[selected], trace.value[selected] * value_factor, np.nan)
    if len(time) > 1:
        gaps = np.flatnonzero(
            np.diff(time) > run.manifest["channels"][signal]["max_gap_s"] * (1 + 1e-9)
        )
        for index in reversed(gaps):
            time = np.insert(time, index + 1, np.nan)
            values = np.insert(values, index + 1, np.nan)
    ax.plot(time / scale, values, style, lw=1.6, label=label or signal.replace("_", " "))


def figures(result, runs, directory):
    directory.mkdir()
    outputs = []
    steps = [
        (run, experiment)
        for run in runs
        for experiment in run.manifest["experiments"]
        if experiment["type"] == "current_step"
    ]
    # Combine only directly comparable reference steps; keep unrelated steps separate.
    groups = {}
    for run, experiment in steps:
        signature = (
            tuple(experiment["window_s"]),
            experiment.get("event_s"),
            tuple(experiment.get("baseline_s", [])),
            tuple(experiment.get("steady_s", [])),
        )
        groups.setdefault(signature, []).append((run, experiment))
    for index, group in enumerate(groups.values(), 1):
        fig, ax = plt.subplots(figsize=(10.5, 5.8))
        for position, (run, experiment) in enumerate(group):
            roles = experiment["signals"]
            if "response" in roles:
                draw_trace(
                    ax,
                    run,
                    roles["response"],
                    experiment["window_s"],
                    run.manifest["run_id"],
                    "-" if position % 2 == 0 else "--",
                    expected_unit="A",
                )
            if "reference" in roles:
                draw_trace(
                    ax,
                    run,
                    roles["reference"],
                    experiment["window_s"],
                    f"Reference: {run.manifest['run_id']}",
                    ":",
                    expected_unit="A",
                )
        ax.set(xlabel="Time (s)", ylabel="Current (A)")
        ax.legend(fontsize=8, loc="best")
        outputs.append(
            save_figure(
                fig,
                directory,
                f"current-response-{index}",
                "Current response and declared tuning settings",
                [run for run, _ in group],
            )
        )
    for run in runs:
        for experiment in run.manifest["experiments"]:
            kind, names, bounds = experiment["type"], experiment["signals"], experiment["window_s"]
            if kind in {"current_step", "torque"}:
                continue
            two = kind in {"battery_limit", "fault", "thermal"}
            fig, axes = plt.subplots(
                2 if two else 1, 1, figsize=(10.5, 7 if two else 5.8), sharex=True
            )
            axes = np.atleast_1d(axes)
            if kind == "disturbance":
                for role in ("reference", "response"):
                    if role in names:
                        draw_trace(
                            axes[0],
                            run,
                            names[role],
                            bounds,
                            role,
                            "--" if role == "reference" else "-",
                            expected_unit="rpm",
                        )
                axes[0].set_ylabel("Shaft speed (rpm)")
                title = "Load-disturbance response"
            elif kind == "battery_limit":
                for axis, roles in zip(axes, (("request", "permitted"), ("current", "limit"))):
                    for role in roles:
                        if role in names:
                            factor = (
                                -1
                                if role == "limit" and experiment.get("direction") == "charge"
                                else 1
                            )
                            draw_trace(
                                axis,
                                run,
                                names[role],
                                bounds,
                                "signed limit" if role == "limit" else role,
                                "--" if role in {"request", "limit"} else "-",
                                value_factor=factor,
                                expected_unit="%" if role in {"request", "permitted"} else "A",
                            )
                axes[0].set_ylabel("Effort (%)")
                axes[1].set_ylabel("Current (A)")
                title = f"Battery {experiment.get('direction', '')} limiting"
            elif kind == "fault":
                for role in ("trigger", "inhibit"):
                    if role in names:
                        draw_trace(
                            axes[0],
                            run,
                            names[role],
                            bounds,
                            role,
                            "--" if role == "trigger" else "-",
                            expected_unit="bool",
                        )
                if "current" in names:
                    draw_trace(
                        axes[1],
                        run,
                        names["current"],
                        bounds,
                        "Measured/declared current",
                        expected_unit="A",
                    )
                axes[0].set_ylabel("Digital state (0/1)")
                axes[1].set_ylabel("Current (A)")
                title = "Fault trigger, inhibit and current decay"
            else:
                for role, name in names.items():
                    unit = run.manifest["channels"][name]["unit"]
                    if unit == "degC":
                        draw_trace(axes[0], run, name, bounds, role.replace("_", " "), scale=60)
                    elif unit == "L/min":
                        draw_trace(axes[1], run, name, bounds, role.replace("_", " "), scale=60)
                axes[0].set_ylabel("Temperature (degC)")
                axes[1].set_ylabel("Flow (L/min)")
                title = "Thermal endurance and coolant flow"
            for axis in axes:
                handles, _ = axis.get_legend_handles_labels()
                if handles:
                    axis.legend(fontsize=8, loc="best")
                if "event_s" in experiment:
                    axis.axvline(experiment["event_s"], color="#6B7280", lw=0.8, ls=":")
            axes[-1].set_xlabel("Time (min)" if kind == "thermal" else "Time (s)")
            outputs.append(
                save_figure(
                    fig, directory, f"{run.manifest['run_id']}-{experiment['id']}", title, [run]
                )
            )
    comparison = result["torque_comparison"]
    observations = [
        row
        for row in comparison.get("observations", [])
        if row["metrics"].get("mean_torque_nm") is not None
    ]
    if observations:
        fig, axes = plt.subplots(1, 2, figsize=(11, 6))
        for position, mode in enumerate(("mg1_only", "mg2_only", "combined")):
            group = [row["metrics"] for row in observations if row["mode"] == mode]
            if group:
                values = [row["mean_torque_nm"] for row in group]
                uncertainty = max((row["torque_expanded_uncertainty_nm"] or 0) for row in group)
                axes[0].scatter(
                    position + np.linspace(-0.07, 0.07, len(group)),
                    values,
                    s=26,
                    color=PALETTE[position],
                )
                axes[0].errorbar(position, np.mean(values), yerr=uncertainty, fmt="ks", capsize=5)
        axes[0].set_xticks([0, 1, 2], ["MG1 only", "MG2 only", "Combined"])
        axes[0].set_ylabel("Output torque (Nm)")
        axes[0].set_title("Trial means; bars are calibration bounds", fontsize=10)
        deltas = comparison.get("paired_differences_nm", [])
        if deltas:
            axes[1].scatter(np.arange(1, len(deltas) + 1), deltas, s=26)
            mean = comparison["mean_increase_nm"]
            bound = comparison["conservative_comparison_bound_nm"]
            axes[1].axhline(mean, color="#225B8A", ls="--", label="Mean increase")
            if bound is not None:
                axes[1].axhspan(
                    mean - bound,
                    mean + bound,
                    color="#225B8A",
                    alpha=0.12,
                    label="Repeatability + calibration bound",
                )
            axes[1].legend(fontsize=8)
        axes[1].axhline(0, color="#6B7280", lw=1)
        axes[1].set(xlabel="Independent matched pair", ylabel="Combined minus MG2 torque (Nm)")
        included = [
            run
            for run in runs
            if any(experiment["type"] == "torque" for experiment in run.manifest["experiments"])
        ]
        # Settings are identical within each accepted pair; list all IDs in the run index.
        outputs.append(
            save_figure(
                fig,
                directory,
                "combined-torque",
                "Single and combined output torque",
                included[:1],
                "Plotted runs: "
                + ", ".join(run.manifest["run_id"] for run in included)
                + "; calibration bounds are not repeatability CIs. Full settings/conditions: run-index.csv",
            )
        )
    return outputs


def flatten(value, prefix=""):
    for key, item in value.items():
        path = f"{prefix}.{key}" if prefix else key
        if isinstance(item, dict):
            yield from flatten(item, path)
        else:
            yield path, item


def metric_unit(name):
    for suffix, unit in (
        ("_nm", "Nm"),
        ("_rpm", "rpm"),
        ("_percent", "%"),
        ("_v", "V"),
        ("_a", "A"),
        ("_w", "W"),
        ("_s", "s"),
        ("_c_per_min", "degC/min"),
        ("_c", "degC"),
    ):
        if name.endswith(suffix):
            return unit
    return "ratio / flag / metadata; see field definition"


def fmt(value, multiplier=1):
    if value is None:
        return "unresolved"
    if isinstance(value, list):
        return "[" + ", ".join(fmt(item, multiplier) for item in value) + "]"
    return f"{value * multiplier:.5g}" if isinstance(value, (int, float)) else str(value)


# Retain unresolved metrics and limitations in Markdown, CSV and JSON rather than inventing results.
def render_report(path, destination):
    result, runs = analyze(path)
    destination = Path(destination)
    destination.mkdir(parents=True, exist_ok=False)
    images = figures(result, runs, destination / "figures")
    result["figures"] = images
    writejson(destination / "analysis.json", result)
    environment = {
        "python": sys.version,
        "bench_version": VERSION,
        "packages": {
            name: importlib.metadata.version(name) for name in ("numpy", "scipy", "matplotlib")
        },
        "source_sha256": {
            str(file.relative_to(Path(__file__).resolve().parents[2])): sha(file)
            for file in Path(__file__).parent.glob("*.py")
        },
    }
    writejson(destination / "environment.json", environment)
    with (destination / "summary.csv").open("w", newline="", encoding="utf-8") as stream:
        writer = csv.writer(stream)
        writer.writerow(
            ["run_id", "experiment", "type", "evidence_class", "status", "metric", "value", "unit"]
        )
        for run in result["runs"]:
            for experiment in run["results"]:
                for metric, value in flatten(experiment["metrics"]):
                    writer.writerow(
                        [
                            run["run_id"],
                            experiment["id"],
                            experiment["type"],
                            run["evidence_class"],
                            experiment["status"],
                            metric,
                            json.dumps(value) if isinstance(value, list) else value,
                            metric_unit(metric),
                        ]
                    )
    with (destination / "run-index.csv").open("w", newline="", encoding="utf-8") as stream:
        writer = csv.writer(stream)
        writer.writerow(
            [
                "run_id",
                "mode",
                "pair_id",
                "evidence_class",
                "manifest",
                "manifest_sha256",
                "settings",
            ]
        )
        for run in runs:
            writer.writerow(
                [
                    run.manifest["run_id"],
                    run.manifest.get("mode"),
                    run.manifest.get("pair_id"),
                    run.manifest["evidence_class"],
                    str(run.path),
                    sha(run.path),
                    json.dumps(run.manifest["controller_settings"], sort_keys=True),
                ]
            )
    lines = [
        "# Prius drivetrain bench results",
        "",
        f"**{classification(result['evidence_class'])}**",
        "",
        "This report analyzes recorded files only. It does not operate hardware, approve a fixture or establish a continuous rating.",
        "",
        "## Method and evidence",
        "",
        "Explicit run windows, units, calibration references and source-clock alignment come from the run manifests. "
        "Raw recordings are preserved. Hashes, settings and measurement boundaries are retained in analysis.json and run-index.csv. "
        "No missing measurement is replaced with simulated data.",
        "",
        "Current rise uses 10-90% of the reference step; settling requires the declared band and hold time. "
        "Insufficient sampling, invalid points, excessive gaps or unverified synchronization leave metrics unresolved. "
        "No automatic gain adjustment is performed.",
        "",
        "## Run status",
        "",
        "| Run | Experiment | Status | Limitations |",
        "| --- | --- | --- | --- |",
    ]
    for run in result["runs"]:
        for experiment in run["results"]:
            notes = (
                "; ".join(experiment["issues"] + run["measurement_warnings"])
                or "None detected by this analysis"
            )
            lines.append(
                f"| {run['run_id']} | {experiment['type']} | {experiment['status']} | {notes} |"
            )
    comparison = result["torque_comparison"]
    lines += [
        "",
        "## Tuning and timing metrics",
        "",
        "| Run | 10-90% rise (ms) | Overshoot (%) | Settling (ms) | Steady error (A) |",
        "| --- | --- | --- | --- | --- |",
    ]
    for run in result["runs"]:
        for experiment in run["results"]:
            metrics = experiment["metrics"]
            if experiment["type"] == "current_step":
                lines.append(
                    f"| {run['run_id']} | {fmt(metrics.get('rise_10_90_s'),1000)} | "
                    f"{fmt(metrics.get('overshoot_percent'))} | {fmt(metrics.get('settling_s'),1000)} | "
                    f"{fmt(metrics.get('steady_error_a'))} |"
                )
    lines += [
        "",
        "| Run | Trigger to inhibit (ms) | Timing upper bound (ms) | Current decay (ms) |",
        "| --- | --- | --- | --- |",
    ]
    for run in result["runs"]:
        for experiment in run["results"]:
            metrics = experiment["metrics"]
            if experiment["type"] == "fault":
                lines.append(
                    f"| {run['run_id']} | {fmt(metrics.get('trigger_to_inhibit_s'),1000)} | "
                    f"{fmt(metrics.get('latency_upper_bound_s'),1000)} | {fmt(metrics.get('current_decay_s'),1000)} |"
                )
    lines += [
        "",
        "## Added-torque comparison",
        "",
        f"Matched independent pairs: **{comparison.get('matched_pairs', 0)}**. "
        f"Mean increase: **{fmt(comparison.get('mean_increase_nm'))} Nm**.",
        "",
        f"Repeatability-only 95% interval: `{fmt(comparison.get('repeatability_95_ci_nm'))}` Nm. "
        f"Calibration bound: `{fmt(comparison.get('calibration_bound_nm'))}` Nm. "
        f"Conservative combined comparison bound: `{fmt(comparison.get('conservative_comparison_bound_nm'))}` Nm.",
        "",
        comparison.get("interpretation", "Comparison unresolved."),
        "",
        "Successive samples are not counted as independent trials. Calibration uncertainty is not divided by the number of samples or runs. "
        "The repeatability CI and calibration bound have different meanings; their sum is a conservative comparison aid, not a single calibrated 95% interval.",
        "",
    ]
    lines.extend(f"- {issue}" for issue in comparison.get("issues", []))
    lines += [
        "",
        "## Figures",
        "",
        "SVGs are available beside the 300 dpi PNGs for scalable presentation export.",
        "",
    ]
    for image in images:
        lines += [
            f"### {image['title']}",
            "",
            f"![{image['title']}]({image['png']})",
            "",
            f"[Vector figure]({image['svg']})",
            "",
        ]
    lines += [
        "## Calibration and limitations",
        "",
        "See [calibration records](calibration.md), [numeric summary](summary.csv), "
        "[run index](run-index.csv), [full analysis](analysis.json) and [software environment](environment.json).",
        "",
        "Efficiency is reported only with synchronized, calibrated physical channels and a declared power boundary. "
        "Its uncertainty is not propagated by this version. Thermal stabilization criteria do not establish allowable winding, magnet or inverter temperatures. "
        "Scope/DAQ timing remains dependent on the declared alignment and sample resolution.",
        "",
        "No road-speed, physical protection, continuous-power or JSF-compliance claim follows from this report alone.",
        "",
    ]
    (destination / "report.md").write_text("\n".join(lines), encoding="utf-8")
    calibration_lines = [
        "# Declared calibration records",
        "",
        f"**{classification(result['evidence_class'])}**",
        "",
        "Entries are supplied metadata, not independently authenticated certificates. Null means missing.",
        "",
        "| Run | Signal | Unit | Expanded uncertainty | Coverage factor | Reference |",
        "| --- | --- | --- | --- | --- | --- |",
    ]
    for run in runs:
        for name, channel in run.manifest["channels"].items():
            cal = channel.get("calibration") or {}
            calibration_lines.append(
                f"| {run.manifest['run_id']} | {name} | {channel['unit']} | "
                f"{cal.get('expanded_uncertainty')} | {cal.get('coverage_factor')} | {cal.get('reference')} |"
            )
    (destination / "calibration.md").write_text(
        "\n".join(calibration_lines) + "\n", encoding="utf-8"
    )
    return destination / "report.md"
