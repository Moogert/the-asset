"""Compare steady loaded MG2-only and combined runs. No invented measurement results."""

import argparse
import csv
import json
import math
import statistics
from pathlib import Path


def summarize(rows, mode):
    group = [r for r in rows if r["mode"] == mode]
    if len(group) < 10:
        raise ValueError(f"Need at least 10 steady samples for {mode}")
    fields = (
        "output_rpm",
        "mg2_command_percent",
        "output_torque_nm",
        "torque_uncertainty_nm",
        "mg1_dc_a",
        "mg2_dc_a",
    )
    values = {key: [float(r[key]) for r in group] for key in fields}
    if any(not math.isfinite(x) for data in values.values() for x in data):
        raise ValueError("Nonfinite sample")
    if (
        min(values["torque_uncertainty_nm"]) <= 0
        or min(values["output_rpm"]) <= 0
        or min(values["output_torque_nm"]) <= 0
    ):
        raise ValueError("Positive forward speed, torque and instrument uncertainty required")
    if any(not 0 <= x <= 100 for x in values["mg2_command_percent"]):
        raise ValueError("MG2 motoring command must be in [0,100]")
    rpm = statistics.mean(values["output_rpm"])
    command = statistics.mean(values["mg2_command_percent"])
    if max(values["output_rpm"]) - min(values["output_rpm"]) > rpm * 0.02:
        raise ValueError(f"{mode}: speed not steady within 2% peak-to-peak")
    if max(values["mg2_command_percent"]) - min(values["mg2_command_percent"]) > 0.1:
        raise ValueError(f"{mode}: MG2 command not steady")
    if any(
        a > 5 and b < -5 or b > 5 and a < -5 for a, b in zip(values["mg1_dc_a"], values["mg2_dc_a"])
    ):
        raise ValueError(f"{mode}: possible circulating electrical power")
    torque = statistics.mean(values["output_torque_nm"])
    # Conservative systematic uncertainty + sample variability, not a certification.
    bound = max(values["torque_uncertainty_nm"]) + 2 * statistics.stdev(
        values["output_torque_nm"]
    ) / math.sqrt(len(group))
    return {
        "samples": len(group),
        "rpm": rpm,
        "mg2_command_percent": command,
        "mean_torque_nm": torque,
        "comparison_bound_nm": bound,
        "mean_power_w": statistics.mean(
            t * n * 2 * math.pi / 60
            for t, n in zip(values["output_torque_nm"], values["output_rpm"])
        ),
    }


def compare(rows):
    baseline, combined = (summarize(rows, m) for m in ("mg2_only", "combined"))
    if abs(baseline["rpm"] - combined["rpm"]) > baseline["rpm"] * 0.01:
        raise ValueError("Runs must match mean output speed within 1%")
    if abs(baseline["mg2_command_percent"] - combined["mg2_command_percent"]) > 0.1:
        raise ValueError("Runs must match MG2 command within 0.1 percentage point")
    delta = combined["mean_torque_nm"] - baseline["mean_torque_nm"]
    bound = baseline["comparison_bound_nm"] + combined["comparison_bound_nm"]
    return {
        "mg2_only": baseline,
        "combined": combined,
        "torque_increase_nm": delta,
        "combined_comparison_bound_nm": bound,
        "increase_exceeds_bound": delta > bound,
        "status": "sample_comparison_only_requires_calibrated_fixture_and_test_review",
    }


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("csv", type=Path)
    args = p.parse_args()
    try:
        with args.csv.open(newline="", encoding="utf-8-sig") as f:
            result = compare(list(csv.DictReader(f)))
    except (ValueError, KeyError, OSError) as error:
        p.exit(2, f"Cannot compare runs: {error}\n")
    print(json.dumps(result, indent=2, allow_nan=False))


if __name__ == "__main__":
    main()
