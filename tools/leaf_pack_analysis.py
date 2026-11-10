"""Compare early 24 kWh Leaf arrangements without declaring hardware suitability."""

import argparse
import copy
import json
from pathlib import Path
from drivetrain_analysis import ROOT, analyze, bounded


# Compare whole early-Leaf modules only; donor SOH, voltage floor and energy window remain assumptions.
def compare_packs(vehicle, study):
    original_s = bounded(study, "original_series_groups", 1, 200)
    original_p = bounded(study, "original_parallel_cells", 1, 10)
    energy = bounded(study, "original_nameplate_kwh", 1, 100)
    nominal = bounded(study, "original_nominal_v", 1, 1000)
    full = bounded(study, "original_full_v", nominal, 1000)
    floor = bounded(study, "assumed_minimum_loaded_v_at_96s", 1, nominal)
    health = bounded(study, "example_remaining_capacity_fraction", 0.01, 1)
    window = bounded(study, "example_operating_energy_window_fraction", 0.01, 1)
    base = analyze(vehicle)
    required = base["no_field_weakening_bus_requirement_v"]["mg2"]
    candidates = []
    for config in study["configurations"]:
        s = bounded(config, "series_groups", 2, 200)
        p = bounded(config, "parallel_cells", 2, 10)
        if int(s) != s or int(p) != p or s % 2 or p % 2 or s * p > original_s * original_p:
            raise ValueError(
                "Early-module comparison requires whole 2s2p modules within one donor's cell count"
            )
        scale = s / original_s
        vnom, vfull, vfloor = nominal * scale, full * scale, floor * scale
        usable = energy * s * p / (original_s * original_p) * health * window
        loads = []
        for grade in (0, 0.03, 0.06):
            v = copy.deepcopy(vehicle)
            v["grade_fraction"] = grade
            power = analyze(v)["road_load_w"]["battery_total"]
            hours = usable / (power / 1000)
            loads.append(
                {
                    "grade_percent": grade * 100,
                    "battery_kw": power / 1000,
                    "current_at_assumed_loaded_floor_a": power / vfloor,
                    "example_energy_only_minutes": hours * 60,
                    "example_energy_only_miles": hours * vehicle["target_mph"],
                    "energy_for_15_minutes_kwh": power / 4000,
                }
            )
        candidates.append(
            {
                "name": config["name"],
                "series_groups": s,
                "parallel_cells": p,
                "early_modules_used": s * p / 4,
                "nominal_v": vnom,
                "full_v": vfull,
                "assumed_loaded_floor_v": vfloor,
                "example_operating_energy_kwh": usable,
                "required_effective_flux_reduction_at_loaded_floor": (
                    None if required is None else max(0, 1 - vfloor / required)
                ),
                "battery_heat_w": None,
                "loads": loads,
            }
        )
    return {
        "status": "architecture_screening_not_a_pack_rating",
        "motor_rpm_at_target": base["locked_planetary_rpm"]["mg2"],
        "mg2_no_fw_bus_screen_v": required,
        "configurations": candidates,
        "engineering_preference": "Preserve original 96-series architecture as starting point; donor capacity/condition and physical integration remain to be selected",
        "limitations": study["limitations"],
    }


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--vehicle", type=Path, default=ROOT / "configs/capstone_vehicle.json")
    p.add_argument("--study", type=Path, default=ROOT / "configs/leaf-pack-study.json")
    p.add_argument("--output", type=Path)
    args = p.parse_args()
    try:
        report = compare_packs(
            json.loads(args.vehicle.read_text()), json.loads(args.study.read_text())
        )
    except (ValueError, KeyError, TypeError, OSError) as error:
        p.exit(2, f"Invalid inputs: {error}\n")
    text = json.dumps(report, indent=2, allow_nan=False) + "\n"
    if args.output:
        args.output.write_text(text, encoding="utf-8")
    else:
        print(text, end="")


if __name__ == "__main__":
    main()
