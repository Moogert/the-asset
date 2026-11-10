"""Conservative recorded-data metrics. Missing evidence yields unresolved results."""

import math
from pathlib import Path

import numpy as np
from scipy import stats

from . import VERSION
from .data import Run, loadjson, number, sha, window


# Check declared channel roles, units, timing and coverage before calculating any metric.
def required(run, experiment, roles):
    names = []
    issues = []
    for role, unit in roles.items():
        name = experiment["signals"].get(role)
        if name is None:
            issues.append(f"Missing {role} channel")
        elif run.manifest["channels"][name]["unit"] != unit:
            issues.append(f"{role}: expected {unit}")
        else:
            names.append(name)
    if not issues:
        issues.extend(run.problems(names, experiment["window_s"]))
    return issues


def unresolved(issues):
    return {"status": "unresolved", "issues": issues, "metrics": {}}


# Use a time-weighted trapezoidal mean so nonuniform sampling does not overweight dense intervals.
def average(time, values):
    return float(np.trapezoid(values, time) / (time[-1] - time[0]))


# Interpolate only a supported crossing; an absent crossing is unresolved, not zero rise time.
def crossing(time, values, target):
    indices = np.flatnonzero(values >= target)
    if not len(indices):
        return None
    index = int(indices[0])
    result = float(time[index])
    if index and values[index] != values[index - 1]:
        result = float(
            time[index - 1]
            + (time[index] - time[index - 1])
            * (target - values[index - 1])
            / (values[index] - values[index - 1])
        )
    return result


# Require every remaining selected sample inside the band plus the declared hold duration.
def settling(time, response, target, band, event, hold):
    outside = np.flatnonzero(np.abs(response - target) > band)
    start = int(outside[-1]) + 1 if len(outside) else 0
    if start >= len(time) or time[-1] - time[start] < hold or len(time[start:]) < 10:
        return None
    return float(time[start] - event)


def event_windows(experiment):
    event = number(experiment.get("event_s"), "event_s")
    baseline = window(experiment.get("baseline_s"), "baseline_s")
    steady = window(experiment.get("steady_s"), "steady_s")
    if baseline[1] >= event or steady[0] <= event:
        raise ValueError("baseline must end before event; steady window must start after event")
    return event, baseline, steady


# Estimate step response only within explicitly declared pre/post windows and sampling constraints.
def step(run, experiment):
    issues = required(run, experiment, {"reference": "A", "response": "A"})
    if issues:
        return unresolved(issues)
    event, baseline, steady = event_windows(experiment)
    reference, response = (experiment["signals"][key] for key in ("reference", "response"))
    issues = run.problems([reference, response], baseline, 5) + run.problems(
        [reference, response], steady
    )
    if issues:
        return unresolved(issues)
    before, after = (run.mean(reference, bounds) for bounds in (baseline, steady))
    amplitude = after - before
    if abs(amplitude) < 1e-9:
        return unresolved(["Reference has no nonzero step"])
    _, reference_values = run.section(reference, steady)
    band_fraction = number(
        experiment.get("settling_fraction", 0.02), "settling_fraction", 0.001, 0.5
    )
    band = abs(amplitude) * band_fraction
    if np.ptp(reference_values) > band:
        return unresolved(["Reference is not steady in the final window"])
    t, y = run.section(response, [event, experiment["window_s"][1]])
    if len(t) < 10:
        return unresolved(["Too few post-event samples"])
    _, baseline_values = run.section(response, baseline)
    noise = float(np.std(baseline_values, ddof=1))
    normalized = (y - before) / amplitude
    first = crossing(t, normalized, 0.1)
    last = crossing(t, normalized, 0.9)
    dynamic_issues = []
    if first is None or last is None or last <= first:
        dynamic_issues.append("10-90% rise not resolved")
    elif np.count_nonzero((t >= first) & (t <= last)) < 10:
        dynamic_issues.append("Fewer than 10 samples across rise; dynamic metrics undersampled")
    if 3 * noise > band:
        dynamic_issues.append(
            "Baseline noise exceeds one third of settling band; dynamic metrics unresolved"
        )
    hold = number(experiment.get("minimum_hold_s", 0.02), "minimum_hold_s", 0, 1e6)
    settle = settling(t, y, after, band, event, hold) if not dynamic_issues else None
    if settle is None and not dynamic_issues:
        dynamic_issues.append("Not settled for the required hold within observation")
    saturation = None
    name = experiment["signals"].get("saturation")
    if name:
        if run.manifest["channels"][name]["unit"] == "bool" and not run.problems(
            [name], experiment["window_s"]
        ):
            st, values = run.section(name, experiment["window_s"])
            saturation = average(st, values)
        else:
            dynamic_issues.append("Saturation channel unavailable/invalid")
    else:
        dynamic_issues.append("Saturation was not recorded")
    resolved = (
        first is not None
        and last is not None
        and last > first
        and not any("undersampled" in issue or "noise" in issue for issue in dynamic_issues)
    )
    return {
        "status": "resolved" if not dynamic_issues else "partial",
        "issues": dynamic_issues,
        "metrics": {
            "step_amplitude_a": amplitude,
            "baseline_noise_std_a": noise,
            "rise_10_90_s": last - first if resolved else None,
            "overshoot_percent": (
                max(0.0, float(np.max(normalized) - 1) * 100) if resolved else None
            ),
            "settling_s": settle,
            "settling_band_a": band,
            "steady_error_a": run.mean(response, steady) - after,
            "saturation_time_fraction": saturation,
        },
    }


# Measure speed deviation/recovery relative to the declared event, with current response as context.
def disturbance(run, experiment):
    issues = required(run, experiment, {"reference": "rpm", "response": "rpm"})
    if issues:
        return unresolved(issues)
    event, baseline, steady = event_windows(experiment)
    reference, response = (experiment["signals"][key] for key in ("reference", "response"))
    issues = run.problems([reference, response], baseline, 5) + run.problems(
        [reference, response], steady
    )
    if issues:
        return unresolved(issues)
    target = run.mean(reference, baseline)
    rt, refs = run.section(reference, experiment["window_s"])
    if np.ptp(refs) > max(abs(target) * 0.001, 1e-6):
        return unresolved(["Disturbance test requires a constant speed reference"])
    time, response_values = run.section(response, [event, experiment["window_s"][1]])
    band = number(
        experiment.get("recovery_band_rpm", max(abs(target) * 0.02, 1)), "recovery band", 1e-9
    )
    recovery = settling(
        time,
        response_values,
        target,
        band,
        event,
        number(experiment.get("minimum_hold_s", 0.1), "hold", 0),
    )
    return {
        "status": "resolved" if recovery is not None else "partial",
        "issues": [] if recovery is not None else ["Recovery not established within observation"],
        "metrics": {
            "maximum_speed_deviation_rpm": float(np.max(np.abs(response_values - target))),
            "recovery_s": recovery,
            "recovery_band_rpm": band,
            "steady_error_rpm": run.mean(response, steady) - run.mean(reference, steady),
        },
    }


# Require synchronized physical inputs and a declared measurement boundary before reporting efficiency.
def power(run, experiment):
    names = experiment["signals"]
    needed = {"torque": "Nm", "speed": "rpm", "voltage": "V", "current": "A"}
    issues = required(run, experiment, needed)
    boundary = run.manifest.get("power_boundary")
    if not boundary or issues:
        return {
            "efficiency": None,
            "issues": issues + ([] if boundary else ["Power measurement boundary missing"]),
        }
    actual = [names[key] for key in needed]
    for name in actual:
        channel = run.manifest["channels"][name]
        if channel["measurement"] != "physical" or channel.get("calibration") is None:
            issues.append(f"{name}: physical calibrated measurement needed for efficiency")
    if issues:
        return {"efficiency": None, "issues": issues}
    time, (torque, speed, voltage, current) = run.aligned(actual, experiment["window_s"])
    mechanical = average(time, torque * speed * 2 * math.pi / 60)
    electrical = average(time, voltage * current)
    if electrical <= 0 or mechanical < 0 or mechanical > electrical:
        return {
            "efficiency": None,
            "issues": ["Motoring power signs/balance inconsistent; inspect measurements"],
        }
    return {
        "efficiency": mechanical / electrical,
        "mechanical_power_w": mechanical,
        "electrical_power_w": electrical,
        "boundary": boundary,
        "issues": [],
        "uncertainty": "Efficiency uncertainty not propagated; do not use as an uncertainty-qualified rating",
    }


# Reduce a steady run to one observation; calibration and circulation checks gate comparison use.
def torque(run, experiment):
    roles = {"torque": "Nm", "speed": "rpm", "mg2_command": "%", "voltage": "V", "mg2_temp": "degC"}
    issues = required(run, experiment, roles)
    if issues:
        return unresolved(issues)
    bounds = experiment["window_s"]
    names = experiment["signals"]
    means = {role: run.mean(names[role], bounds) for role in roles}
    if means["speed"] <= 0 or means["torque"] <= 0 or means["voltage"] <= 0:
        return unresolved(["Positive forward speed/torque/voltage required for this comparison"])
    speed_values = run.section(names["speed"], bounds)[1]
    command_values = run.section(names["mg2_command"], bounds)[1]
    if np.ptp(speed_values) > 0.02 * means["speed"] or np.ptp(command_values) > 0.1:
        return unresolved(
            ["Steady window exceeds 2% speed span or 0.1 percentage point MG2 command span"]
        )
    voltage_span = number(
        experiment.get("max_voltage_span_fraction", 0.02), "voltage span fraction", 0
    )
    temperature_span = number(experiment.get("max_temperature_span_c", 5), "temperature span", 0)
    if (
        np.ptp(run.section(names["voltage"], bounds)[1]) > voltage_span * means["voltage"]
        or np.ptp(run.section(names["mg2_temp"], bounds)[1]) > temperature_span
    ):
        return unresolved(["Voltage/temperature are not steady within the declared study limits"])
    if np.min(command_values) < 0 or np.max(command_values) > 100:
        return unresolved(["MG2 command must be in [0,100] for motoring comparison"])
    calibration = run.manifest["channels"][names["torque"]].get("calibration")
    if calibration is None:
        issues.append("Torque calibration/expanded uncertainty missing")
    elif calibration["expanded_uncertainty"] <= 0:
        issues.append("Positive torque calibration uncertainty required")
    if run.manifest["channels"][names["torque"]]["measurement"] != "physical":
        issues.append("Output torque is not declared a physical measurement")
    circulation = None
    if all(key in names for key in ("mg1_current", "mg2_current")):
        current_issues = required(run, experiment, {"mg1_current": "A", "mg2_current": "A"})
        if current_issues:
            issues.extend(current_issues)
        else:
            _, (first, second) = run.aligned([names["mg1_current"], names["mg2_current"]], bounds)
            threshold = number(
                experiment.get("circulation_threshold_a", 5), "circulation threshold", 0
            )
            circulation = bool(
                np.any(
                    ((first > threshold) & (second < -threshold))
                    | ((second > threshold) & (first < -threshold))
                )
            )
            if circulation:
                issues.append("Opposite channel currents exceed the declared study threshold")
    else:
        issues.append("Separate channel DC currents missing; circulation not assessed")
    time, (torque_values, speed) = run.aligned([names["torque"], names["speed"]], bounds)
    return {
        "status": "resolved" if not issues else "partial",
        "issues": issues,
        "metrics": {
            "mean_torque_nm": means["torque"],
            "mean_speed_rpm": means["speed"],
            "mean_mg2_command_percent": means["mg2_command"],
            "mean_voltage_v": means["voltage"],
            "mean_mg2_temp_c": means["mg2_temp"],
            "mechanical_power_w": average(time, torque_values * speed * 2 * math.pi / 60),
            "torque_expanded_uncertainty_nm": (
                calibration["expanded_uncertainty"] if calibration else None
            ),
            "torque_coverage_factor": calibration["coverage_factor"] if calibration else None,
            "circulation_flag": circulation,
            "power": power(run, experiment),
        },
    }


# Compare signed measured current with the permitted charge/discharge envelope.
def battery(run, experiment):
    roles = {"request": "%", "permitted": "%", "current": "A", "limit": "A"}
    issues = required(run, experiment, roles)
    if issues:
        return unresolved(issues)
    if experiment.get("direction") not in {"discharge", "charge"}:
        raise ValueError("battery_limit direction must be discharge or charge")
    time, (request, permitted, current, limit) = run.aligned(
        [experiment["signals"][key] for key in roles], experiment["window_s"]
    )
    if np.min(limit) < 0 or np.max(np.abs(request)) > 100 or np.max(np.abs(permitted)) > 100:
        return unresolved(["Negative current limit or invalid effort range"])
    directed_current = current if experiment["direction"] == "discharge" else -current
    excess = np.maximum(0, directed_current - limit)
    zero = limit == 0
    return {
        "status": "resolved",
        "issues": ["Numerical tracking result; not a physical protection acceptance"],
        "metrics": {
            "maximum_current_excess_a": float(np.max(excess)),
            "estimated_time_above_limit_s": float(np.sum(np.diff(time) * (excess[:-1] > 0))),
            "maximum_permitted_effort_at_zero_limit_percent": (
                float(np.max(np.abs(permitted[zero]))) if np.any(zero) else None
            ),
            "sample_resolution_s": float(np.max(np.diff(time))),
        },
    }


def rising_edge(time, values, event):
    positions = np.flatnonzero((values[:-1] < 0.5) & (values[1:] >= 0.5) & (time[1:] >= event))
    if not len(positions):
        return None
    index = int(positions[0]) + 1
    return float(time[index]), float(time[index] - time[index - 1])


# Require physical trigger/inhibit timing evidence; CAN timestamps alone cannot establish gate latency.
def fault(run, experiment):
    roles = {"trigger": "bool", "inhibit": "bool", "current": "A"}
    issues = required(run, experiment, roles)
    if issues:
        return unresolved(issues)
    event = number(experiment.get("event_s"), "event_s")
    names = experiment["signals"]
    channels = [run.manifest["channels"][names[role]] for role in ("trigger", "inhibit")]
    physical = all(
        channel["measurement"] == "physical"
        and run.manifest["sources"][channel["source"]]["kind"] in {"scope", "daq"}
        for channel in channels
    )
    if not physical:
        return unresolved(
            [
                "Physical trigger/inhibit scope or DAQ measurements required; CAN/software flags are insufficient"
            ]
        )
    trigger = rising_edge(*run.section(names["trigger"], experiment["window_s"]), event)
    inhibit = rising_edge(*run.section(names["inhibit"], experiment["window_s"]), event)
    if trigger is None or inhibit is None or inhibit[0] < trigger[0]:
        return unresolved(["Trigger/inhibit rising edge missing or ordering invalid"])
    timing_error = (
        trigger[1]
        + inhibit[1]
        + sum(
            run.manifest["sources"][channel["source"]]["alignment_uncertainty_s"]
            for channel in channels
        )
    )
    latency = inhibit[0] - trigger[0]
    upper = latency + timing_error
    time, current = run.section(names["current"], [trigger[0], experiment["window_s"][1]])
    zero_band = number(experiment.get("current_zero_band_a", 1), "current zero band", 1e-9)
    decay = settling(
        time,
        current,
        0,
        zero_band,
        trigger[0],
        number(experiment.get("minimum_hold_s", 0.02), "hold", 0),
    )
    current_channel = run.manifest["channels"][names["current"]]
    current_calibration = current_channel.get("calibration")
    decay_issue = []
    if (
        current_channel["measurement"] != "physical"
        or not current_calibration
        or current_calibration["expanded_uncertainty"] >= zero_band
    ):
        decay = None
        decay_issue.append(
            "Physical current calibration uncertainty must be below the declared zero-current band"
        )
    eligible = run.manifest["evidence_class"] == "measured"
    return {
        "status": "resolved" if decay is not None else "partial",
        "issues": (
            [] if decay is not None else ["Current decay not established within observation"]
        )
        + decay_issue
        + ([] if eligible else ["Non-measured evidence: no physical shutdown claim"]),
        "metrics": {
            "trigger_to_inhibit_s": latency,
            "timing_error_bound_s": timing_error,
            "latency_upper_bound_s": upper,
            "current_decay_s": decay,
            "measured_latency_within_100ms": upper <= 0.1 if eligible else None,
        },
    }


# Check both final slope and span; estimated heat flow is not a proven continuous hardware rating.
def thermal(run, experiment):
    signals = experiment["signals"]
    temperature_roles = [
        role for role in signals if role.endswith("temp") or role in {"coolant_in", "coolant_out"}
    ]
    if not temperature_roles:
        return unresolved(["Temperature channels missing"])
    issues = required(run, experiment, {role: "degC" for role in temperature_roles})
    if issues:
        return unresolved(issues)
    steady = window(experiment.get("steady_s"), "thermal steady_s")
    threshold = number(experiment.get("stabilization_c_per_min", 0.1), "stabilization slope", 1e-9)
    span_limit = number(experiment.get("stabilization_span_c", 1), "stabilization span", 1e-9)
    minimum = number(experiment.get("minimum_steady_seconds", 300), "minimum steady duration", 1)
    results = {}
    for role in temperature_roles:
        name = signals[role]
        time, values = run.section(name, experiment["window_s"])
        st, sy = run.section(name, steady)
        quality = run.problems([name], steady)
        slope = float(stats.linregress(st, sy).slope * 60) if not quality else None
        adequate = not quality and st[-1] - st[0] >= minimum
        results[role] = {
            "initial_c": float(values[0]),
            "final_c": float(values[-1]),
            "rise_c": float(values[-1] - values[0]),
            "maximum_c": float(np.max(values)),
            "final_slope_c_per_min": slope,
            "final_window_span_c": float(np.ptp(sy)) if len(sy) else None,
            "stabilization_span_limit_c": span_limit,
            "stabilized_by_study_criterion": (
                bool(abs(slope) <= threshold and float(np.ptp(sy)) <= span_limit)
                if adequate
                else None
            ),
        }
        issues.extend(quality)
        if not adequate:
            issues.append(f"{role}: insufficient final-window duration for stabilization claim")
    heat = None
    if all(key in signals for key in ("coolant_in", "coolant_out", "flow")):
        flow_issues = required(
            run, experiment, {"flow": "L/min", "coolant_in": "degC", "coolant_out": "degC"}
        )
        density = experiment.get("coolant_density_kg_l")
        cp = experiment.get("coolant_cp_j_kg_k")
        if not flow_issues and density is not None and cp is not None:
            time, (flow, inlet, outlet) = run.aligned(
                [signals[key] for key in ("flow", "coolant_in", "coolant_out")],
                experiment["window_s"],
            )
            if np.min(flow) >= 0:
                heat = average(
                    time,
                    flow
                    / 60
                    * number(density, "coolant density", 0.1, 3)
                    * number(cp, "coolant cp", 100, 10000)
                    * (outlet - inlet),
                )
                issues.append(
                    "Coolant heat transfer is an estimate without propagated flow/temperature uncertainty"
                )
        else:
            issues.extend(
                flow_issues + ["Coolant heat estimate needs declared density and heat capacity"]
            )
    return {
        "status": "resolved" if not issues else "partial",
        "issues": issues,
        "metrics": {
            "temperatures": results,
            "mean_coolant_heat_w": heat,
            "continuous_hardware_rating_established": False,
        },
    }


ANALYZERS = {
    "current_step": step,
    "disturbance": disturbance,
    "torque": torque,
    "battery_limit": battery,
    "fault": fault,
    "thermal": thermal,
}


def analyze_run(run):
    results = []
    for experiment in run.manifest["experiments"]:
        result = ANALYZERS[experiment["type"]](run, experiment)
        result.update(
            {
                "id": experiment["id"],
                "type": experiment["type"],
                "signals": experiment["signals"],
                "window_s": experiment["window_s"],
            }
        )
        results.append(result)
    return {
        "run_id": run.manifest["run_id"],
        "evidence_class": run.manifest["evidence_class"],
        "measurement_warnings": [
            f"{name}: physical channel has no declared calibration"
            for name, channel in run.manifest["channels"].items()
            if channel["measurement"] == "physical" and channel.get("calibration") is None
        ],
        "controller_settings": run.manifest["controller_settings"],
        "hardware": run.manifest["hardware"],
        "firmware": run.manifest["firmware"],
        "fixture_id": run.manifest["fixture_id"],
        "provenance": run.provenance,
        "measurement_channels": run.manifest["channels"],
        "sources": run.manifest["sources"],
        "timebase": run.manifest["timebase"],
        "operating_conditions": run.manifest["operating_conditions"],
        "experiment_protocols": run.manifest["experiments"],
        "calibrations": {
            name: channel.get("calibration") for name, channel in run.manifest["channels"].items()
        },
        "results": results,
    }


# Pair independent runs at matched conditions; samples within one run are not independent trials.
def compare_torque(runs, reports, policy):
    pairs = {}
    issues = []
    observations = []
    for run, report in zip(runs, reports):
        for result in report["results"]:
            if result["type"] != "torque":
                continue
            observations.append(
                {"run_id": report["run_id"], "mode": run.manifest.get("mode"), **result}
            )
            mode = run.manifest.get("mode")
            if mode not in {"mg2_only", "combined"}:
                continue
            pair = run.manifest.get("pair_id")
            if not pair or mode in pairs.setdefault(pair, {}):
                raise ValueError("Torque comparison needs unique pair_id/mode combinations")
            pairs[pair][mode] = (run, result)
    if not pairs:
        return {
            "status": "unresolved",
            "issues": ["No paired MG2-only/combined runs"],
            "observations": observations,
        }
    defaults = {
        "speed_fraction": 0.01,
        "command_percentage_points": 0.1,
        "voltage_fraction": 0.02,
        "temperature_c": 5,
    }
    tolerances = {key: number(policy.get(key, value), key, 0) for key, value in defaults.items()}
    # One torque difference per independent pair; calibration error is kept separate from repeatability.
    differences = []
    systematic = []
    for pair, modes in pairs.items():
        if set(modes) != {"mg2_only", "combined"}:
            issues.append(f"{pair}: missing paired condition")
            continue
        baseline_run, baseline = modes["mg2_only"]
        combined_run, combined = modes["combined"]
        if baseline["status"] != "resolved" or combined["status"] != "resolved":
            issues.append(f"{pair}: unresolved measurement/calibration/circulation evidence")
            continue
        first, second = baseline["metrics"], combined["metrics"]
        conditions = (
            ("mean_speed_rpm", tolerances["speed_fraction"] * first["mean_speed_rpm"]),
            ("mean_mg2_command_percent", tolerances["command_percentage_points"]),
            ("mean_voltage_v", tolerances["voltage_fraction"] * first["mean_voltage_v"]),
            ("mean_mg2_temp_c", tolerances["temperature_c"]),
        )
        if any(abs(first[key] - second[key]) > allowed + 1e-10 for key, allowed in conditions):
            issues.append(f"{pair}: operating conditions do not match declared tolerances")
            continue
        if (
            baseline_run.manifest["firmware"] != combined_run.manifest["firmware"]
            or baseline_run.manifest["controller_settings"]
            != combined_run.manifest["controller_settings"]
            or baseline_run.manifest["fixture_id"] != combined_run.manifest["fixture_id"]
        ):
            issues.append(f"{pair}: firmware/settings/fixture differ")
            continue
        differences.append(second["mean_torque_nm"] - first["mean_torque_nm"])
        systematic.append(
            first["torque_expanded_uncertainty_nm"] + second["torque_expanded_uncertainty_nm"]
        )
    count = len(differences)
    if count < 5:
        issues.append("At least five independent paired runs required for the final comparison")
    mean = float(np.mean(differences)) if count else None
    margin = (
        float(stats.t.ppf(0.975, count - 1) * np.std(differences, ddof=1) / math.sqrt(count))
        if count > 1
        else None
    )
    # Unknown calibration-error correlation: retain a conservative bound instead of RSS cancellation.
    calibration = max(systematic) if systematic else None
    bound = margin + calibration if margin is not None else None
    evidence = runs[0].manifest["evidence_class"]
    eligible = evidence == "measured" and not issues and bound is not None
    if evidence == "measured":
        for run in runs:
            if any(experiment["type"] == "torque" for experiment in run.manifest["experiments"]):
                recorded = [
                    run.manifest[key] for key in ("hardware", "firmware", "controller_settings")
                ]
                if (
                    not run.manifest.get("fixture_id")
                    or any(not item for item in recorded)
                    or any(
                        value is None or value == "" for item in recorded for value in item.values()
                    )
                ):
                    issues.append(
                        f"{run.manifest['run_id']}: hardware/firmware/settings/fixture traceability incomplete"
                    )
        eligible = not issues and bound is not None
    return {
        "status": "resolved" if not issues else "unresolved",
        "issues": issues,
        "observations": observations,
        "matched_pairs": count,
        "paired_differences_nm": differences,
        "mean_increase_nm": mean,
        "repeatability_95_ci_nm": [mean - margin, mean + margin] if margin is not None else None,
        "calibration_bound_nm": calibration,
        "conservative_comparison_bound_nm": bound,
        "correlation_policy": "Calibration errors not reduced by repeat count; unknown correlation bounded by addition",
        "measured_added_torque_supported": bool(eligible and mean > bound),
        "evidence_class": evidence,
        "tolerances": tolerances,
        "interpretation": (
            "Numerical comparison only; fixture and calibration review required"
            if eligible
            else "No measured-performance conclusion: evidence is non-measured or requirements remain open"
        ),
    }


def load_input(path):
    path = Path(path).resolve()
    document = loadjson(path)
    if not isinstance(document, dict):
        raise ValueError("Input must be a run or campaign JSON object")
    if "runs" in document:
        if document.get("schema_version") != 1 or not document["runs"]:
            raise ValueError("Campaign schema_version=1 and nonempty runs required")
        runs = [Run(path.parent / name) for name in document["runs"]]
    else:
        runs = [Run(path)]
    if len({run.manifest["run_id"] for run in runs}) != len(runs):
        raise ValueError("Duplicate run IDs; repeated files are not independent trials")
    if len({run.manifest["evidence_class"] for run in runs}) != 1:
        raise ValueError("Mixed evidence classes in a campaign are prohibited")
    if runs[0].manifest["evidence_class"] == "measured":
        seen = set()
        for run in runs:
            captures = {capture["sha256"] for capture in run.manifest["captures"]}
            if captures & seen:
                raise ValueError(
                    "A capture is reused across measured runs; independent acquisition not established"
                )
            seen.update(captures)
    return runs, document


def analyze(path):
    runs, document = load_input(path)
    reports = [analyze_run(run) for run in runs]
    result = {
        "schema_version": 1,
        "tool_version": VERSION,
        "offline_only": True,
        "evidence_class": runs[0].manifest["evidence_class"],
        "input_sha256": sha(path),
        "runs": reports,
        "torque_comparison": compare_torque(
            runs, reports, document.get("comparison_tolerances", {})
        ),
        "limitations": [
            "No hardware commands or automatic gain changes",
            "Reported metrics depend on declared calibration and timing evidence",
            "No continuous rating, physical safety approval or JSF compliance established",
        ],
    }
    return result, runs
