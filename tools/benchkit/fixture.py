"""Two-output absorber sizing concept. No structural or equipment approval."""

import copy
import math

from .data import number
from drivetrain_analysis import analyze


# Calculate equal-speed two-output load requirements; allowance factors are not verified equipment ratings.
def size_fixture(vehicle, study):
    margin = number(study.get("study_margin", 1.25), "study_margin", 1, 3)
    delta = number(study.get("absorber_water_rise_k", 10), "absorber water rise", 1, 50)
    points = []
    for mph in study["speeds_mph"]:
        for grade in study["grades"]:
            config = copy.deepcopy(vehicle)
            config["target_mph"] = number(mph, "speed", 0.1, 200)
            config["grade_fraction"] = number(grade, "grade", 0, 0.3)
            calculated = analyze(config)
            motor_rpm = calculated["locked_planetary_rpm"]["mg2"]
            output_rpm = motor_rpm / config["final_drive"]
            total_w = calculated["road_load_w"]["wheel_total"]
            per_side_w = total_w / 2
            torque = per_side_w / (output_rpm * 2 * math.pi / 60)
            points.append(
                {
                    "speed_mph": mph,
                    "grade_fraction": grade,
                    "motor_rpm": motor_rpm,
                    "each_output_rpm": output_rpm,
                    "total_absorption_w": total_w,
                    "each_absorber_w": per_side_w,
                    "each_absorber_torque_nm": torque,
                    "study_margin_each_absorber_w": per_side_w * margin,
                    "study_margin_each_absorber_torque_nm": torque * margin,
                    "study_margin_each_absorber_water_l_min": per_side_w
                    * margin
                    / (4180 * delta)
                    * 60,
                }
            )
    if not points:
        raise ValueError("Nonempty speed/grade study required")
    peak = study.get("peak_combined_motor_torque_nm")
    peak_output = (
        None if peak is None else number(peak, "peak torque", 0) * vehicle["final_drive"] / 2
    )
    inertia = study.get("equivalent_motor_inertia_kg_m2")
    energy = (
        None
        if inertia is None
        else 0.5
        * number(inertia, "inertia", 0)
        * (max(p["motor_rpm"] for p in points) * 2 * math.pi / 60) ** 2
    )
    return {
        "status": "concept_only_not_a_rated_fixture",
        "topology": "two_equal_speed_instrumented_output_absorbers",
        "assumptions": [
            "Planetary members locked; differential retains normal operation",
            "Both output shafts rotate at equal signed road-equivalent speeds",
            "Steady road load split equally; transaxle losses rejected through its own cooling system",
            "Water calculation uses 4180 J/kg/K and 1 kg/L; not a vendor flow specification",
            "Study margin is not a structural safety factor or a transient rating",
        ],
        "points": points,
        "study_margin": margin,
        "required_peak_each_output_torque_nm_if_supplied": peak_output,
        "stored_rotating_energy_j_if_inertia_supplied": energy,
        "equipment_ratings_verified": False,
        "unresolved": [
            "Actual motor torque-speed/current envelope",
            "Output adapter dimensions, CV support and alignment",
            "Coupling, bearings, mounting, fatigue, imbalance and containment review",
            "Selected absorber continuous/intermittent torque-speed curves and minimum speed",
            "Low-speed/standstill torque restraint capability",
            "Absorber cooling and fault-stop energy",
            "Both-output speed regulation and differential overspeed protection",
            "Emergency inhibition, regeneration acceptance and residual PM back-EMF",
            "Calibrated torque/speed/current/voltage instrumentation",
        ],
    }


def fixture_markdown(report):
    lines = [
        "# Two-output load-fixture concept",
        "",
        "**CONCEPT ONLY - calculated study requirements; no equipment or structure is approved.**",
        "",
        "Retain normal differential operation. Couple each supported output to an instrumented, commercially rated absorber. "
        "Coordinate the two loads to hold equal road-equivalent output speeds. Do not hold one output stationary and reuse these speed calculations.",
        "",
        "| mph | grade | motor rpm | each output rpm | each absorber kW | each absorber Nm | with study margin kW |",
        "| --- | --- | --- | --- | --- | --- | --- |",
    ]
    for point in report["points"]:
        lines.append(
            f"| {point['speed_mph']:g} | {point['grade_fraction']*100:g}% | {point['motor_rpm']:.0f} | "
            f"{point['each_output_rpm']:.0f} | {point['each_absorber_w']/1000:.2f} | "
            f"{point['each_absorber_torque_nm']:.1f} | {point['study_margin_each_absorber_w']/1000:.2f} |"
        )
    lines += [
        "",
        "The listed points are steady road-load cases. They do not cover stall/launch torque, faults, transient overload or stopping energy. "
        "Peak-torque and inertia inputs remain unknown unless supplied; no peak rating is inferred from motor nameplates.",
        "",
        "A procurement specification must require a manufacturer's torque-speed envelope, continuous/intermittent ratings, cooling requirements, "
        "speed feedback and documented behavior on loss of power/load. Eddy-current absorption generally weakens toward zero speed; "
        "standstill tests need separately reviewed holding capability.",
        "",
        "## Review gates",
        "",
    ]
    lines.extend(f"- {item}." for item in report["unresolved"])
    lines += [
        "",
        "The normal Civic motor/inverter coolant loop is not assumed to absorb the dynamometer's full load heat. "
        "The absorbers need their own rated heat-rejection arrangement. Select parts and complete faculty/lab mechanical and electrical review before fabrication or powered operation.",
        "",
    ]
    return "\n".join(lines)
