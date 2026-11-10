"""Analytic synthetic traces exercise the full import/report workflow.

These are not a validated Prius plant model or measured tuning results.
"""

import csv
from pathlib import Path

import numpy as np

from .data import import_capture, writejson


def base_manifest(run_id, channels, experiments):
    metadata = {}
    for name, unit in channels.items():
        metadata[name] = {
            "source": "daq",
            "unit": unit,
            "max_gap_s": 0.0002,
            "measurement": "command" if "ref" in name or "command" in name else "physical",
            "calibration": {
                "expanded_uncertainty": 0.5 if unit == "Nm" else 0.01,
                "coverage_factor": 2,
                "reference": "SYNTHETIC-DEMO-NOT-A-CERTIFICATE",
            },
        }
    return {
        "schema_version": 1,
        "run_id": run_id,
        "evidence_class": "synthetic_demo",
        "hardware": {"controller": "analytic synthetic generator; no hardware"},
        "firmware": {"version": "synthetic fixture v1"},
        "fixture_id": "SYNTHETIC-NOT-BUILT",
        "controller_settings": {
            "profile": "synthetic_nominal",
            "gain_units": "illustrative; not firmware settings",
        },
        "operating_conditions": {
            "description": "Analytic demonstration, not measured motor behavior"
        },
        "timebase": {"reference": "synthetic_clock", "max_alignment_uncertainty_s": 0.001},
        "sources": {
            "daq": {
                "kind": "daq",
                "clock_id": "synthetic_clock",
                "offset_s": 0,
                "synchronized": True,
                "alignment_uncertainty_s": 0.00001,
            }
        },
        "channels": metadata,
        "experiments": experiments,
        "captures": [],
    }


def emit(destination, manifest, time, values):
    inputs = destination / "inputs" / manifest["run_id"]
    inputs.mkdir(parents=True)
    raw = inputs / "instrument.csv"
    with raw.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.writer(stream)
        writer.writerow(["Time_ms", *values])
        writer.writerows(zip(time * 1000, *(values[name] for name in values)))
    template = inputs / "manifest-template.json"
    writejson(template, manifest)
    mapping = {
        "source": "daq",
        "time_column": "Time_ms",
        "time_unit": "ms",
        "channels": {
            name: {"signal": name, "unit": manifest["channels"][name]["unit"]} for name in values
        },
    }
    writejson(inputs / "mapping.json", mapping)
    return import_capture(
        raw, inputs / "mapping.json", template, destination / "runs" / manifest["run_id"]
    )


# Create analytic synthetic data only; these traces are not a validated Prius plant or measured trial.
def make_demo(destination):
    destination = Path(destination)
    destination.mkdir(parents=True, exist_ok=False)
    manifests = []
    for label, damping, frequency in (("before", 0.35, 300), ("after", 0.8, 500)):
        time = np.arange(0, 0.30001, 0.0001)
        age = np.maximum(0, time - 0.05)
        wd = frequency * np.sqrt(1 - damping**2)
        response = 10 * (
            1
            - np.exp(-damping * frequency * age)
            * (np.cos(wd * age) + damping / np.sqrt(1 - damping**2) * np.sin(wd * age))
        )
        experiment = {
            "id": "current_step",
            "type": "current_step",
            "window_s": [0, 0.3],
            "event_s": 0.05,
            "baseline_s": [0, 0.04],
            "steady_s": [0.25, 0.3],
            "minimum_hold_s": 0.02,
            "settling_fraction": 0.02,
            "signals": {"reference": "iq_ref", "response": "iq_actual", "saturation": "saturation"},
        }
        manifest = base_manifest(
            f"tuning_{label}", {"iq_ref": "A", "iq_actual": "A", "saturation": "bool"}, [experiment]
        )
        manifest["controller_settings"] = {
            "profile": f"illustrative_{label}",
            "kp": 1 if label == "before" else 2,
            "ki": 10 if label == "before" else 20,
            "gain_units": "arbitrary demo units; NOT OpenInverter gains",
        }
        manifests.append(
            emit(
                destination,
                manifest,
                time,
                {
                    "iq_ref": np.where(time >= 0.05, 10.0, 0),
                    "iq_actual": response,
                    "saturation": np.zeros_like(time),
                },
            )
        )
    for mode in ("mg2_only", "combined", "mg1_only"):
        for index in range(5 if mode != "mg1_only" else 1):
            time = np.arange(0, 2.0001, 0.01)
            nominal = (
                60 + 0.2 * index
                if mode == "mg2_only"
                else 80 + 0.2 * index + [0, 0.1, -0.1, 0.2, -0.2][index]
            )
            if mode == "mg1_only":
                nominal = 20.0
            torque = nominal + 0.05 * np.sin(2 * np.pi * time)
            speed = np.full_like(time, 1000.0)
            electrical_a = torque * speed * 2 * np.pi / 60 / 0.9 / 350
            mg2_current = (
                np.zeros_like(time)
                if mode == "mg1_only"
                else np.full_like(time, 60 * 1000 * 2 * np.pi / 60 / 0.9 / 350)
            )
            values = {
                "output_torque": torque,
                "output_speed": speed,
                "mg2_command": np.full_like(time, 0 if mode == "mg1_only" else 40),
                "pack_voltage": np.full_like(time, 350),
                "pack_current": electrical_a,
                "mg2_temp": np.full_like(time, 50),
                "mg1_current": electrical_a - mg2_current,
                "mg2_current": mg2_current,
            }
            units = {
                "output_torque": "Nm",
                "output_speed": "rpm",
                "mg2_command": "%",
                "pack_voltage": "V",
                "pack_current": "A",
                "mg2_temp": "degC",
                "mg1_current": "A",
                "mg2_current": "A",
            }
            experiment = {
                "id": "torque",
                "type": "torque",
                "window_s": [0, 2],
                "signals": {
                    "torque": "output_torque",
                    "speed": "output_speed",
                    "mg2_command": "mg2_command",
                    "voltage": "pack_voltage",
                    "current": "pack_current",
                    "mg2_temp": "mg2_temp",
                    "mg1_current": "mg1_current",
                    "mg2_current": "mg2_current",
                },
            }
            manifest = base_manifest(f"{mode}_{index+1}", units, [experiment])
            manifest.update(
                {
                    "mode": mode,
                    "pair_id": f"pair_{index+1}",
                    "power_boundary": "SYNTHETIC DC-link input to combined output shafts; auxiliaries excluded",
                }
            )
            for channel in manifest["channels"].values():
                channel["max_gap_s"] = 0.011
            manifests.append(emit(destination, manifest, time, values))
    time = np.arange(0, 5.0001, 0.005)
    age = np.maximum(0, time - 1)
    exp = {
        "id": "load_step",
        "type": "disturbance",
        "window_s": [0, 5],
        "event_s": 1,
        "baseline_s": [0, 0.9],
        "steady_s": [4, 5],
        "recovery_band_rpm": 20,
        "signals": {"reference": "speed_ref", "response": "speed_actual"},
    }
    manifest = base_manifest("load_disturbance", {"speed_ref": "rpm", "speed_actual": "rpm"}, [exp])
    for channel in manifest["channels"].values():
        channel["max_gap_s"] = 0.006
    manifests.append(
        emit(
            destination,
            manifest,
            time,
            {
                "speed_ref": np.full_like(time, 1000),
                "speed_actual": 1000 - 80 * (1 - np.exp(-age / 0.03)) * np.exp(-age / 0.35),
            },
        )
    )
    for direction in ("charge", "discharge"):
        time = np.arange(0, 2.0001, 0.001)
        sign = 1 if direction == "discharge" else -1
        limit = np.where(time < 0.5, 50.0, np.where(time < 1, 20.0, 0.0))
        current = np.where(
            time < 0.5,
            50.0,
            np.where(
                time < 1,
                20 + 30 * np.exp(-np.maximum(0, time - 0.5) / 0.02),
                20 * np.exp(-np.maximum(0, time - 1) / 0.02),
            ),
        )
        exp = {
            "id": "battery",
            "type": "battery_limit",
            "direction": direction,
            "window_s": [0, 2],
            "event_s": 0.5,
            "signals": {
                "request": "request",
                "permitted": "permitted",
                "current": "pack_current",
                "limit": "limit",
            },
        }
        manifest = base_manifest(
            f"battery_{direction}",
            {"request": "%", "permitted": "%", "pack_current": "A", "limit": "A"},
            [exp],
        )
        for channel in manifest["channels"].values():
            channel["max_gap_s"] = 0.0011
        manifests.append(
            emit(
                destination,
                manifest,
                time,
                {
                    "request": np.full_like(time, sign * 80),
                    "permitted": sign * limit * 1.6,
                    "pack_current": sign * current,
                    "limit": limit,
                },
            )
        )
    time = np.arange(0, 0.30001, 0.0001)
    exp = {
        "id": "feedback_loss",
        "type": "fault",
        "window_s": [0, 0.3],
        "event_s": 0.1,
        "current_zero_band_a": 1,
        "minimum_hold_s": 0.02,
        "signals": {"trigger": "trigger", "inhibit": "inhibit", "current": "motor_current"},
    }
    manifest = base_manifest(
        "fault_timing", {"trigger": "bool", "inhibit": "bool", "motor_current": "A"}, [exp]
    )
    manifests.append(
        emit(
            destination,
            manifest,
            time,
            {
                "trigger": (time >= 0.1).astype(int),
                "inhibit": (time >= 0.125).astype(int),
                "motor_current": 30 * np.exp(-np.maximum(0, time - 0.125) / 0.003),
            },
        )
    )
    time = np.arange(0, 1801.0)
    exp = {
        "id": "endurance",
        "type": "thermal",
        "window_s": [0, 1800],
        "steady_s": [1200, 1800],
        "minimum_steady_seconds": 300,
        "stabilization_c_per_min": 0.1,
        "coolant_density_kg_l": 1.05,
        "coolant_cp_j_kg_k": 3600,
        "signals": {
            "oil_temp": "oil_temp",
            "winding_temp": "winding_temp",
            "inverter_temp": "inverter_temp",
            "coolant_in": "coolant_in",
            "coolant_out": "coolant_out",
            "flow": "flow",
        },
    }
    units = {
        key: "degC"
        for key in ("oil_temp", "winding_temp", "inverter_temp", "coolant_in", "coolant_out")
    }
    units["flow"] = "L/min"
    manifest = base_manifest("thermal_endurance", units, [exp])
    for channel in manifest["channels"].values():
        channel["max_gap_s"] = 1.01
    manifests.append(
        emit(
            destination,
            manifest,
            time,
            {
                "oil_temp": 35 + 45 * (1 - np.exp(-time / 220)),
                "winding_temp": 35 + 75 * (1 - np.exp(-time / 180)),
                "inverter_temp": 35 + 20 * (1 - np.exp(-time / 150)),
                "coolant_in": 35 + 10 * (1 - np.exp(-time / 220)),
                "coolant_out": 37.5 + 10 * (1 - np.exp(-time / 220)),
                "flow": np.full_like(time, 10),
            },
        )
    )
    campaign = destination / "campaign.json"
    writejson(
        campaign,
        {
            "schema_version": 1,
            "description": "SYNTHETIC DEMONSTRATION ONLY - no physical motor results",
            "runs": [str(path.relative_to(destination)).replace("\\", "/") for path in manifests],
        },
    )
    return campaign
