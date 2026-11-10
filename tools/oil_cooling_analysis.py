"""Steady winding/oil/coolant thermal network; sensitivity estimates only."""

import argparse
import json
import math
from pathlib import Path
from drivetrain_analysis import bounded, analyze, ROOT
from thermal_analysis import thermal_case


# Use counterflow effectiveness-NTU, including the equal-capacity limit, to obtain W/K conductance.
def exchanger_conductance(ua, hot_capacity_rate, cold_capacity_rate):
    if any(not math.isfinite(x) or x < 0 for x in (ua, hot_capacity_rate, cold_capacity_rate)):
        raise ValueError("UA and fluid capacity rates must be finite and nonnegative")
    if min(ua, hot_capacity_rate, cold_capacity_rate) == 0:
        return 0.0
    cmin, cmax = min(hot_capacity_rate, cold_capacity_rate), max(
        hot_capacity_rate, cold_capacity_rate
    )
    ratio, ntu = cmin / cmax, ua / cmin
    if abs(1 - ratio) < 1e-8:
        effectiveness = ntu / (1 + ntu)
    else:
        decay = math.exp(-ntu * (1 - ratio))
        effectiveness = (1 - decay) / (1 - ratio * decay)
    return effectiveness * cmin


def oil_case(cooling, scenario, oil, option, shaft_w):
    base = thermal_case(cooling, scenario, shaft_w)
    pump = bounded(option, "pump_heat_w", 0, 1000)
    oil_flow = bounded(option, "external_oil_flow_l_min", 0, 50)
    ua = bounded(option, "added_cooler_ua_w_k", 0, 2000)
    density = bounded(oil, "oil_density_kg_l", 0.5, 1.5)
    cp = bounded(oil, "oil_cp_j_kg_k", 500, 4000)
    coolant_rate = (
        cooling["coolant_flow_l_min"]
        / 60
        * cooling["coolant_density_kg_l"]
        * cooling["coolant_cp_j_kg_k"]
    )
    added_g = exchanger_conductance(ua, oil_flow / 60 * density * cp, coolant_rate)
    # Abstract conductance utilization, not a physical valve position or pump PWM.
    utilization = (
        bounded(option, "cooler_conductance_utilization", 0, 1)
        if "cooler_conductance_utilization" in option
        else 1.0
    )
    added_g *= utilization
    stock_g = bounded(oil, "stock_oil_to_coolant_conductance_w_k", 0.001, 2000)
    # Account for pump heat in the same radiator heat budget without double counting.
    coolant = base["coolant_hot_c"] + pump / scenario["radiator_ua_w_k"] + pump / (2 * coolant_rate)
    numerator = base["gear_loss_w"] + pump
    denominator = stock_g + added_g
    paths = {}
    for motor in ("mg1", "mg2"):
        gc = 1 / bounded(oil, f"{motor}_direct_winding_to_coolant_k_w", 0.001, 10)
        go = 1 / bounded(oil, f"{motor}_winding_to_oil_k_w", 0.001, 10)
        loss = base["motors"][motor]["loss_w"]
        numerator += loss * go / (gc + go)
        denominator += gc * go / (gc + go)
        paths[motor] = (gc, go, loss)
    oil_delta = numerator / denominator
    oil_temp = coolant + oil_delta
    motors = {}
    total_direct_heat = 0.0
    for motor, (gc, go, loss) in paths.items():
        temp = coolant + (loss + go * oil_delta) / (gc + go)
        to_oil = go * (temp - oil_temp)
        direct = gc * (temp - coolant)
        motors[motor] = {
            "winding_c": temp,
            "loss_w": loss,
            "to_oil_w": to_oil,
            "direct_to_casing_w": direct,
        }
        total_direct_heat += direct
    oil_rejected = oil_delta * (stock_g + added_g)
    winding_limit = bounded(cooling, "winding_design_limit_c", cooling["ambient_c"] + 1, 180)
    derate = bounded(oil, "oil_planning_derate_c", cooling["ambient_c"] + 1, 150)
    stop = bounded(oil, "oil_planning_stop_c", derate + 1, 160)
    winding_peak = max(m["winding_c"] for m in motors.values())
    return {
        "bulk_oil_c": oil_temp,
        "local_hot_coolant_c": coolant,
        "motors": motors,
        "peak_winding_c": winding_peak,
        "added_cooler_effective_conductance_w_k": added_g,
        "added_cooler_heat_w": added_g * oil_delta,
        "stock_oil_path_heat_w": stock_g * oil_delta,
        "total_heat_w": base["total_heat_w"] + pump,
        "oil_node_balance_error_w": base["gear_loss_w"]
        + pump
        + sum(m["to_oil_w"] for m in motors.values())
        - oil_rejected,
        "system_heat_balance_error_w": total_direct_heat
        + oil_rejected
        + base["inverter_loss_w"]
        - base["total_heat_w"]
        - pump,
        "above_proposed_oil_derate": oil_temp >= derate,
        "above_proposed_oil_stop": oil_temp >= stop,
        "within_assumed_steady_limits": oil_temp < derate
        and winding_peak <= winding_limit
        and coolant <= cooling["coolant_design_limit_c"]
        and coolant + base["inverter_loss_w"] * scenario["inverter_case_to_coolant_k_w"]
        <= cooling["inverter_case_design_limit_c"],
    }


def oil_report(vehicle, cooling, oil):
    wheel = analyze(vehicle)["road_load_w"]["wheel_total"]
    cases = {}
    for label, scenario in cooling["scenarios"].items():
        shaft = wheel / scenario["gear_efficiency"]
        cases[label] = {}
        target = bounded(oil, "oil_control_screen_target_c", cooling["ambient_c"] + 1, 120)
        for name, option in oil["options"].items():
            result = oil_case(cooling, scenario, oil, option, shaft)
            bypass = dict(option, cooler_conductance_utilization=0.0)
            bypass_result = oil_case(cooling, scenario, oil, bypass, shaft)
            feasible = result["bulk_oil_c"] <= target <= bypass_result["bulk_oil_c"]
            control = {
                "target_c": target,
                "target_bracketed_by_available_cooling": feasible,
                "note": "Effective-conductance fraction only; not a valve-position or pump-PWM command. Oil target does not guarantee winding limits.",
            }
            if feasible:
                low, high = 0.0, 1.0
                for _ in range(45):
                    fraction = (low + high) / 2
                    trial = oil_case(
                        cooling,
                        scenario,
                        oil,
                        dict(option, cooler_conductance_utilization=fraction),
                        shaft,
                    )
                    if trial["bulk_oil_c"] > target:
                        low = fraction
                    else:
                        high = fraction
                controlled = oil_case(
                    cooling, scenario, oil, dict(option, cooler_conductance_utilization=high), shaft
                )
                control.update(
                    {
                        "effective_conductance_fraction": high,
                        "predicted_oil_c": controlled["bulk_oil_c"],
                        "predicted_peak_winding_c": controlled["peak_winding_c"],
                        "within_assumed_steady_limits": controlled["within_assumed_steady_limits"],
                    }
                )
            result["thermostatic_control_screen"] = control
            cases[label][name] = result
    return {
        "status": "unvalidated_oil_network_estimate",
        "scenarios": cases,
        "assumptions": oil["assumptions"],
        "note": "No automatic pump/cooler selection or optimum oil temperature claim. Adequate oil distribution must be verified independently of bulk temperature.",
    }


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--vehicle", type=Path, default=ROOT / "configs/capstone_vehicle.json")
    p.add_argument("--cooling", type=Path, default=ROOT / "configs/civic-radiator-estimate.json")
    p.add_argument("--oil", type=Path, default=ROOT / "configs/transaxle-oil-estimate.json")
    p.add_argument("--output", type=Path)
    args = p.parse_args()
    try:
        report = oil_report(
            *(
                json.loads(path.read_text(encoding="utf-8"))
                for path in (args.vehicle, args.cooling, args.oil)
            )
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
