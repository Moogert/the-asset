"""Lumped steady/transient thermal sensitivity analysis for a Civic radiator loop."""

import argparse
import json
import math
from pathlib import Path
from drivetrain_analysis import analyze, bounded, ROOT


# Combine assumed losses and lumped thermal paths; UA and thermal resistances require measurement.
def thermal_case(c, s, shaft_w):
    ambient = bounded(c, "ambient_c", -20, 60)
    flow = bounded(c, "coolant_flow_l_min", 0.1, 100)
    density = bounded(c, "coolant_density_kg_l", 0.5, 2)
    cp = bounded(c, "coolant_cp_j_kg_k", 1000, 5000)
    share = bounded(c, "mg1_shaft_power_share", 0, 1)
    duration = bounded(c, "transient_seconds", 0, 86400)
    ua = bounded(s, "radiator_ua_w_k", 1, 5000)
    motor_eta = bounded(s, "motor_efficiency", 0.1, 1)
    inverter_eta = bounded(s, "inverter_efficiency", 0.1, 1)
    gear_eta = bounded(s, "gear_efficiency", 0.1, 1)
    if not math.isfinite(shaft_w) or shaft_w < 0:
        raise ValueError("shaft_w must be finite and nonnegative")
    copper_and_core = shaft_w * (1 / motor_eta - 1)
    inverter = shaft_w / motor_eta * (1 / inverter_eta - 1)
    gear = shaft_w * (1 - gear_eta)
    heat = copper_and_core + inverter + gear
    capacity_rate = flow / 60 * density * cp
    coolant_mean = ambient + heat / ua
    coolant_hot = coolant_mean + heat / (2 * capacity_rate)
    winding_limit = bounded(c, "winding_design_limit_c", ambient + 1, 180)
    coolant_limit = bounded(c, "coolant_design_limit_c", ambient + 1, 100)
    inverter_limit = bounded(c, "inverter_case_design_limit_c", ambient + 1, 150)
    r_inv = bounded(s, "inverter_case_to_coolant_k_w", 0.0001, 1)
    case_temp = coolant_hot + inverter * r_inv
    motors = {}
    for motor, fraction in (("mg1", share), ("mg2", 1 - share)):
        resistance = bounded(s, f"{motor}_winding_to_coolant_k_w", 0.0001, 2)
        capacity = bounded(s, f"{motor}_effective_heat_capacity_j_k", 10, 1000000)
        loss = copper_and_core * fraction
        steady = coolant_hot + resistance * loss
        tau = resistance * capacity
        after = steady + (ambient - steady) * math.exp(-duration / tau)
        time_limit = (
            None
            if steady <= winding_limit
            else tau * math.log((steady - ambient) / (steady - winding_limit))
        )
        motors[motor] = {
            "shaft_w": shaft_w * fraction,
            "loss_w": loss,
            "steady_winding_c": steady,
            "time_constant_s": tau,
            "winding_after_duration_c": after,
            "time_to_winding_limit_s": time_limit,
        }
    passes = (
        max(m["steady_winding_c"] for m in motors.values()) <= winding_limit
        and coolant_hot <= coolant_limit
        and case_temp <= inverter_limit
    )
    return {
        "shaft_w": shaft_w,
        "wheel_w": shaft_w * gear_eta,
        "traction_dc_w": shaft_w / motor_eta / inverter_eta,
        "total_heat_w": heat,
        "motor_loss_w": copper_and_core,
        "inverter_loss_w": inverter,
        "gear_loss_w": gear,
        "coolant_mean_c": coolant_mean,
        "coolant_hot_c": coolant_hot,
        "coolant_rise_k": heat / capacity_rate,
        "inverter_case_c": case_temp,
        "motors": motors,
        "within_assumed_steady_limits": passes,
    }


def thermal_report(vehicle, config):
    road = analyze(vehicle)
    wheel = road["road_load_w"]["wheel_total"]
    cases = {}
    for label, s in config["scenarios"].items():
        eta = bounded(s, "gear_efficiency", 0.1, 1)
        cruise = thermal_case(config, s, wheel / eta)
        low, high = 0.0, 200000.0
        for _ in range(60):
            mid = (low + high) / 2
            if thermal_case(config, s, mid)["within_assumed_steady_limits"]:
                low = mid
            else:
                high = mid
        cases[label] = {
            "at_target_road_load": cruise,
            "thermal_only_shaft_ceiling_kw_not_power_rating": low / 1000,
            "radiator_ua_w_k_assumed": s["radiator_ua_w_k"],
        }
    return {
        "status": "physics_sensitivity_estimate_not_hardware_validation",
        "radiator": config["radiator"],
        "scenarios": cases,
        "assumptions": config["assumptions"],
        "limits": "Constant efficiency and linear thermal resistance; no torque-speed/current/voltage saturation, magnet hotspot, oil-flow map, or fan/radiator performance map. Transient assumes coolant immediately reaches modeled hot temperature.",
    }


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--vehicle", type=Path, default=ROOT / "configs/capstone_vehicle.json")
    p.add_argument("--cooling", type=Path, default=ROOT / "configs/civic-radiator-estimate.json")
    p.add_argument("--output", type=Path)
    args = p.parse_args()
    try:
        report = thermal_report(
            json.loads(args.vehicle.read_text()), json.loads(args.cooling.read_text())
        )
    except (ValueError, KeyError, OSError, TypeError) as error:
        p.exit(2, f"Invalid inputs: {error}\n")
    text = json.dumps(report, indent=2, allow_nan=False) + "\n"
    if args.output:
        args.output.write_text(text, encoding="utf-8")
    else:
        print(text, end="")


if __name__ == "__main__":
    main()
