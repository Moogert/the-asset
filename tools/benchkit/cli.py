"""Offline-only CLI. No device enumeration, serial imports or CAN output."""

import argparse
import csv
import json
from pathlib import Path

from .data import import_captures, loadjson, writejson
from .fixture import fixture_markdown, size_fixture

ROOT = Path(__file__).resolve().parents[2]


def create_plan(destination, vehicle, study):
    destination = Path(destination)
    sizing = size_fixture(loadjson(vehicle), loadjson(study))
    destination.mkdir(parents=True, exist_ok=False)
    writejson(destination / "fixture-sizing.json", sizing)
    (destination / "fixture-concept.md").write_text(fixture_markdown(sizing), encoding="utf-8")
    definitions = [
        (
            "current_step",
            {"reference": "A", "response": "A", "saturation": "bool"},
            {
                "window_s": [0, 0.3],
                "event_s": 0.05,
                "baseline_s": [0, 0.04],
                "steady_s": [0.25, 0.3],
                "settling_fraction": 0.02,
                "minimum_hold_s": 0.02,
            },
            0.0002,
        ),
        (
            "disturbance",
            {"reference": "rpm", "response": "rpm"},
            {
                "window_s": [0, 5],
                "event_s": 1,
                "baseline_s": [0, 0.9],
                "steady_s": [4, 5],
                "minimum_hold_s": 0.1,
                "recovery_band_rpm": 20,
            },
            0.006,
        ),
        (
            "torque",
            {
                "torque": "Nm",
                "speed": "rpm",
                "mg2_command": "%",
                "voltage": "V",
                "current": "A",
                "mg2_temp": "degC",
                "mg1_current": "A",
                "mg2_current": "A",
            },
            {"window_s": [0, 2]},
            0.011,
        ),
        (
            "battery_limit",
            {"request": "%", "permitted": "%", "current": "A", "limit": "A"},
            {"window_s": [0, 2], "event_s": 0.5, "direction": "discharge"},
            0.0011,
        ),
        (
            "fault",
            {"trigger": "bool", "inhibit": "bool", "current": "A"},
            {
                "window_s": [0, 0.3],
                "event_s": 0.1,
                "current_zero_band_a": 1,
                "minimum_hold_s": 0.02,
            },
            0.0002,
        ),
        (
            "thermal",
            {
                "oil_temp": "degC",
                "winding_temp": "degC",
                "inverter_temp": "degC",
                "coolant_in": "degC",
                "coolant_out": "degC",
                "flow": "L/min",
            },
            {
                "window_s": [0, 1800],
                "steady_s": [1200, 1800],
                "minimum_steady_seconds": 300,
                "stabilization_c_per_min": 0.1,
                "coolant_density_kg_l": None,
                "coolant_cp_j_kg_k": None,
            },
            1.01,
        ),
    ]
    (destination / "templates").mkdir()
    for kind, roles, fields, gap in definitions:
        manifest = {
            "schema_version": 1,
            "run_id": f"{kind}_run01",
            "evidence_class": "measured",
            "hardware": {
                "mcu": None,
                "transaxle": None,
                "inverter": None,
                "board_revision": None,
                "instrument": None,
            },
            "firmware": {"motor_controller_hash": None, "supervisor_hash": None},
            "fixture_id": None,
            "controller_settings": {"profile": None},
            "operating_conditions": {"voltage_v": None, "ambient_c": None},
            "timebase": {"reference": "unconfirmed", "max_alignment_uncertainty_s": 0.001},
            "sources": {
                "instrument": {
                    "kind": "scope" if kind in {"current_step", "fault"} else "daq",
                    "clock_id": "unconfirmed",
                    "offset_s": None,
                    "alignment_uncertainty_s": None,
                    "synchronized": False,
                }
            },
            "channels": {
                name: {
                    "source": "instrument",
                    "unit": unit,
                    "calibration": None,
                    "max_gap_s": gap,
                    "measurement": (
                        "command"
                        if name in {"reference", "request", "permitted", "limit", "mg2_command"}
                        else "physical"
                    ),
                }
                for name, unit in roles.items()
            },
            "experiments": [
                {"id": kind, "type": kind, **fields, "signals": {name: name for name in roles}}
            ],
            "captures": [],
        }
        if kind == "torque":
            manifest.update({"mode": "mg2_only", "pair_id": "pair01", "power_boundary": None})
        writejson(destination / "templates" / f"{kind}-manifest.json", manifest)
        mapping = {
            "source": "instrument",
            "time_column": "Time_s",
            "time_unit": "s",
            "channels": {
                name: {"signal": name, "unit": unit, "scale": 1, "offset": 0}
                for name, unit in roles.items()
            },
        }
        writejson(destination / "templates" / f"{kind}-mapping.json", mapping)
    writejson(
        destination / "campaign-template.json",
        {
            "schema_version": 1,
            "runs": [],
            "comparison_tolerances": {
                "speed_fraction": 0.01,
                "command_percentage_points": 0.1,
                "voltage_fraction": 0.02,
                "temperature_c": 5,
            },
        },
    )
    with (destination / "test-matrix.csv").open("w", newline="", encoding="utf-8") as stream:
        writer = csv.writer(stream)
        writer.writerow(["experiment", "stage", "required_evidence", "final_repetition_target"])
        writer.writerows(
            [
                [
                    "current_step",
                    "individual MG2 then MG1",
                    "synchronized reference/current/saturation; actual firmware gain scaling",
                    5,
                ],
                [
                    "disturbance",
                    "after current-loop commissioning",
                    "repeatable controlled load; independent shaft speed",
                    5,
                ],
                [
                    "torque",
                    "bounded combined load",
                    "matched MG1-only/MG2-only/combined; calibrated torque; five independent paired trials",
                    5,
                ],
                [
                    "battery_limit",
                    "logic-only first; loaded only under approved limits",
                    "requested/permitted effort and signed pack current",
                    5,
                ],
                [
                    "fault",
                    "logic-only first",
                    "scope trigger, physical inhibit and current; timing uncertainty",
                    5,
                ],
                [
                    "thermal",
                    "after bounded loaded tests",
                    "stable load; oil/winding/inverter/coolant temperatures and flow",
                    "repeat at selected conditions",
                ],
            ]
        )
    return destination / "fixture-concept.md"


# Dispatch offline file workflows only; no command opens a traction-hardware transport.
def main(argv=None):
    parser = argparse.ArgumentParser(
        description="Offline Prius bench planning, CSV import and analysis. No hardware outputs."
    )
    commands = parser.add_subparsers(dest="command", required=True)
    plan = commands.add_parser(
        "plan", help="Create fixture sizing and unfilled measurement templates"
    )
    plan.add_argument("--out", type=Path, required=True)
    plan.add_argument("--vehicle", type=Path, default=ROOT / "configs/capstone_vehicle.json")
    plan.add_argument("--study", type=Path, default=ROOT / "configs/bench-fixture-study.json")
    importer = commands.add_parser(
        "import", help="Import a CSV with an explicit mapping and preserve raw input"
    )
    for name in ("csv", "mapping"):
        importer.add_argument(
            f"--{name}",
            type=Path,
            required=True,
            action="append",
            help="Repeat CSV/mapping pairs in matching order for multiple files",
        )
    for name in ("manifest", "out"):
        importer.add_argument(f"--{name}", type=Path, required=True)
    analyzer = commands.add_parser("analyze", help="Analyze a run manifest or campaign")
    analyzer.add_argument("--input", type=Path, required=True)
    analyzer.add_argument("--output", type=Path)
    report = commands.add_parser("report", help="Produce Markdown, SVG, PNG, JSON and CSV outputs")
    report.add_argument("--input", type=Path, required=True)
    report.add_argument("--out", type=Path, required=True)
    demo = commands.add_parser(
        "demo", help="Exercise all experiment types using clearly marked synthetic traces"
    )
    demo.add_argument("--out", type=Path, required=True)
    args = parser.parse_args(argv)
    try:
        if args.command == "plan":
            print(create_plan(args.out, args.vehicle, args.study))
        elif args.command == "import":
            print(import_captures(args.csv, args.mapping, args.manifest, args.out))
        elif args.command == "analyze":
            from .analysis import analyze

            result, _ = analyze(args.input)
            if args.output:
                with args.output.open("x", encoding="utf-8") as stream:
                    stream.write(json.dumps(result, indent=2, allow_nan=False) + "\n")
            else:
                print(json.dumps(result, indent=2, allow_nan=False))
        elif args.command == "report":
            from .report import render_report

            print(render_report(args.input, args.out))
        else:
            from .demo import make_demo
            from .report import render_report

            campaign = make_demo(args.out)
            print(render_report(campaign, args.out / "report"))
    except (ValueError, KeyError, TypeError, OSError, csv.Error) as error:
        parser.exit(2, f"Cannot complete offline bench operation: {error}\n")
