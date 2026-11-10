"""Planning calculation only. Does not predict peak power or certify road capability."""

import argparse
import json
import math
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def bounded(c, key, low, high, optional=False):
    x = c.get(key)
    if x is None and optional:
        return None
    if (
        isinstance(x, bool)
        or not isinstance(x, (int, float))
        or not math.isfinite(x)
        or not low <= x <= high
    ):
        raise ValueError(f"{key} must be finite and in [{low}, {high}]")
    return x


# Use SI road-load/kinematics and explicit voltage assumptions; a design screen is not a motor rating.
def analyze(c):
    speed = bounded(c, "target_mph", 0, 200) * 0.44704
    tire = bounded(c, "rolling_diameter_m", 0.1, 2)
    ratio = bounded(c, "final_drive", 0.1, 30)
    sun = bounded(c, "sun_teeth", 1, 500)
    ring = bounded(c, "ring_teeth", 1, 500)
    mass = bounded(c, "mass_kg", 100, 10000)
    drag = bounded(c, "drag_area_m2", 0.01, 10)
    roll = bounded(c, "rolling_coefficient", 0, 0.1)
    air = bounded(c, "air_density_kg_m3", 0.5, 2)
    grade = bounded(c, "grade_fraction", 0, 0.3)
    efficiency = bounded(c, "drivetrain_efficiency", 0.1, 1)
    aux = bounded(c, "auxiliary_w", 0, 20000)
    utilization = bounded(c, "voltage_utilization", 0.1, 1)
    pack_v = bounded(c, "pack_min_v", 1, 1000, True)
    pack_a = bounded(c, "pack_discharge_limit_a", 0, 5000, True)
    rpm = speed * 60 / (math.pi * tire) * ratio
    angle = math.atan(grade)
    aero_w = 0.5 * air * drag * speed**3
    rolling_w = roll * mass * 9.80665 * math.cos(angle) * speed
    grade_w = mass * 9.80665 * math.sin(angle) * speed
    wheel_w = aero_w + rolling_w + grade_w
    battery_w = wheel_w / efficiency + aux
    gates = {}
    emf = {}
    uncertainty = (
        bounded(c, "back_emf_relative_uncertainty", 0, 0.5)
        if "back_emf_relative_uncertainty" in c
        else 0
    )
    required_bus = []
    for motor in ("mg1", "mg2"):
        limit = bounded(c, f"{motor}_limit_rpm", 1, 20000, True)
        k = bounded(c, f"{motor}_back_emf_line_rms_v_per_rpm", 0.000001, 10, True)
        gates[f"{motor}_mechanical_speed"] = None if limit is None else rpm < limit
        required = None if k is None else math.sqrt(2) * k * rpm / utilization
        required_bus.append(required)
        gates[f"{motor}_no_field_weakening_voltage"] = (
            None if required is None or pack_v is None else pack_v >= required
        )
        peak_k = bounded(c, f"{motor}_back_emf_line_peak_v_per_rpm", 0.000001, 10, True)
        emf[motor] = {
            "estimated_line_rms_v": None if k is None else k * rpm,
            "estimated_line_peak_v": None if peak_k is None else peak_k * rpm,
            "no_fw_bus_sensitivity_v": (
                None
                if required is None
                else [required * (1 - uncertainty), required * (1 + uncertainty)]
            ),
            "required_effective_flux_reduction_by_bus": (
                None
                if required is None
                else {
                    str(bus): max(0, 1 - bus / required) for bus in (250, 300, 350, 400, 450, 500)
                }
            ),
        }
    gates["pack_steady_state_power"] = (
        None if pack_v is None or pack_a is None else pack_v * pack_a >= battery_w
    )
    return {
        "status": "planning_only_not_road_validated",
        "locked_planetary_rpm": {"mg1": rpm, "mg2": rpm, "carrier": rpm},
        "fixed_carrier_rpm": {"mg1": -ring / sun * rpm, "mg2": rpm, "carrier": 0},
        "road_load_w": {
            "aerodynamic": aero_w,
            "rolling": rolling_w,
            "grade": grade_w,
            "wheel_total": wheel_w,
            "battery_total": battery_w,
        },
        "estimated_pack_current_a": None if pack_v is None else battery_w / pack_v,
        "no_field_weakening_bus_requirement_v": dict(zip(("mg1", "mg2"), required_bus)),
        "back_emf_estimates": emf,
        "back_emf_basis": c.get(
            "back_emf_basis", "User coefficient; measurement/source basis unspecified"
        ),
        "gates": gates,
        "unresolved": [k for k, v in gates.items() if v is None]
        + [
            "measured_continuous_thermal_capability",
            "measured_loaded_torque_speed_envelope",
            "mechanical_and_vehicle_review",
        ],
        "assumptions": c.get("assumptions", []),
        "note": "Voltage check uses ideal SVPWM Vll,rms=Vdc/sqrt(2), with utilization margin; total waveform RMS is only a screening proxy for fundamental RMS. Flux-reduction fractions are voltage ratios, not safe field-weakening current settings. Harmonic open-circuit line peaks can exceed the sinusoidal estimate. No field-weakening credit in gates. Passing is not a loaded torque guarantee.",
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config", type=Path, default=ROOT / "configs/capstone_vehicle.json")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    try:
        result = analyze(json.loads(args.config.read_text(encoding="utf-8")))
    except (ValueError, OSError, TypeError) as error:
        parser.exit(2, f"Invalid inputs: {error}\n")
    text = json.dumps(result, indent=2, allow_nan=False) + "\n"
    if args.output:
        args.output.write_text(text, encoding="utf-8")
    else:
        print(text, end="")


if __name__ == "__main__":
    main()
