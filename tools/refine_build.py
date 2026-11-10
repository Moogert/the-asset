"""Apply known build details to a COPY of the vehicle model and sweep grade/voltage."""

import argparse
import copy
import json
import math
import re
from pathlib import Path
from drivetrain_analysis import ROOT, bounded, analyze
from oil_cooling_analysis import oil_report


def tire_geometry(code):
    match = re.fullmatch(
        r"(?:P)?(\d{3})/(\d{2})\s*[Rr](\d{2})(?:\s+\d{2,3}[A-Za-z])?", code.strip()
    )
    if not match:
        raise ValueError(
            "Use a tire code such as 185/65R14; measured rolling circumference is preferred"
        )
    width, aspect, rim = map(int, match.groups())
    if not 100 <= width <= 400 or not 20 <= aspect <= 90 or not 10 <= rim <= 30:
        raise ValueError("Tire size outside supported range")
    diameter = rim * 0.0254 + 2 * (width / 1000) * (aspect / 100)
    return {
        "unloaded_geometric_diameter_m": diameter,
        "unloaded_geometric_circumference_m": math.pi * diameter,
        "note": "Geometric diameter is not measured loaded rolling circumference; no arbitrary correction factor applied",
    }


def refine(base, inputs, cooling, oil):
    vehicle = copy.deepcopy(base)
    radiator = copy.deepcopy(cooling)
    known = []
    tire = inputs["tires"]
    geometry = tire_geometry(tire["sidewall_code"]) if tire.get("sidewall_code") else None
    circumference = bounded(tire, "measured_loaded_rolling_circumference_m", 0.5, 4, True)
    if circumference is not None:
        vehicle["rolling_diameter_m"] = circumference / math.pi
        known.append("Measured loaded rolling circumference used")
    elif geometry:
        vehicle["rolling_diameter_m"] = geometry["unloaded_geometric_diameter_m"]
        known.append("Sidewall-derived unloaded tire diameter used provisionally")
    else:
        known.append("Tire unknown: retained explicit example diameter")
    battery = inputs["battery"]
    vmin = bounded(battery, "minimum_loaded_v", 1, 1000, True)
    vmax = bounded(battery, "maximum_charged_v", 1, 1000, True)
    nominal = bounded(battery, "nominal_v", 1, 1000, True)
    if vmin is not None and vmax is not None and vmin > vmax:
        raise ValueError("Loaded minimum exceeds charged maximum")
    if nominal is not None and (
        (vmin is not None and nominal < vmin) or (vmax is not None and nominal > vmax)
    ):
        raise ValueError("Nominal voltage outside supplied range")
    if vmin is not None:
        vehicle["pack_min_v"] = vmin
    limit = bounded(battery, "continuous_discharge_a", 0, 5000, True)
    if limit is not None:
        vehicle["pack_discharge_limit_a"] = limit
    duty = inputs["duty"]
    vehicle["target_mph"] = bounded(duty, "continuous_speed_mph", 0, 200)
    grade = bounded(duty, "sustained_grade_fraction", 0, 0.3, True)
    if grade is not None:
        vehicle["grade_fraction"] = grade
    radiator["ambient_c"] = bounded(duty, "ambient_c", -20, 60)
    installed = bounded(inputs["cooling"], "measured_installed_coolant_flow_l_min", 0.1, 100, True)
    if installed is not None:
        radiator["coolant_flow_l_min"] = installed
    # Radiator dimensions identify fitment; they do not establish heat-transfer UA.
    grades = sorted(set([0, 0.03, 0.06] + ([] if grade is None else [grade])))
    sweeps = []
    for hill in grades:
        v = copy.deepcopy(vehicle)
        v["grade_fraction"] = hill
        result = analyze(v)
        power = result["road_load_w"]["battery_total"]
        sweeps.append(
            {
                "grade_percent": hill * 100,
                "estimated_battery_kw": power / 1000,
                "current_by_loaded_bus_v": {
                    str(voltage): power / voltage for voltage in (250, 300, 350, 400)
                },
                "oil_network": oil_report(v, radiator, oil)["scenarios"]["nominal_airflow"],
            }
        )
    missing = []
    for group in ("tires", "radiator", "cooling", "battery", "duty", "orientation"):
        missing.extend(f"{group}.{k}" for k, v in inputs[group].items() if v is None)
    return {
        "status": "provisional_build_refinement_not_hardware_rating",
        "used_inputs": known,
        "tire_geometry": geometry,
        "effective_vehicle_model": vehicle,
        "radiator_build_details": inputs["radiator"],
        "grade_sweep": sweeps,
        "maximum_charged_voltage_needs_component_review": vmax,
        "missing_inputs": missing,
        "notes": [
            "Voltage columns are illustrative screening cases, not pack recommendations",
            "Radiator dimensions are never automatically converted to UA",
            "Assumed current/thermal maps do not demonstrate grade endurance",
        ],
    }


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--inputs", type=Path, default=ROOT / "configs/build-inputs.json")
    p.add_argument("--output", type=Path)
    args = p.parse_args()
    try:

        def load(name):
            return json.loads((ROOT / "configs" / name).read_text(encoding="utf-8"))

        result = refine(
            load("capstone_vehicle.json"),
            json.loads(args.inputs.read_text(encoding="utf-8")),
            load("civic-radiator-estimate.json"),
            load("transaxle-oil-estimate.json"),
        )
    except (ValueError, KeyError, TypeError, OSError) as error:
        p.exit(2, f"Invalid inputs: {error}\n")
    text = json.dumps(result, indent=2, allow_nan=False) + "\n"
    if args.output:
        args.output.write_text(text, encoding="utf-8")
    else:
        print(text, end="")


if __name__ == "__main__":
    main()
