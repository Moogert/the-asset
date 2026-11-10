"""Bench analysis tests use synthetic traces only, including metadata fault cases."""

import copy
import csv
import json
import math
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

import numpy as np

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from benchkit.analysis import analyze, analyze_run, power, step
from benchkit.cli import create_plan
from benchkit.data import (
    COLUMNS,
    Run,
    import_capture,
    import_captures,
    loadjson,
    sha,
    validate_manifest,
    writejson,
)
from benchkit.demo import base_manifest, make_demo
from benchkit.fixture import size_fixture


class BenchTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.shared = tempfile.TemporaryDirectory()
        cls.demo = make_demo(Path(cls.shared.name) / "demo")
        cls.result, cls.runs = analyze(cls.demo)

    @classmethod
    def tearDownClass(cls):
        cls.shared.cleanup()

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.directory = Path(self.temp.name)

    def write_run(self, name="analytic", dt=0.0001, tau=0.01, mutate=None):
        time = np.arange(0, 0.600001, dt)
        values = 10 * (1 - np.exp(-np.maximum(0, time - 0.1) / tau))
        experiment = {
            "id": "step",
            "type": "current_step",
            "window_s": [0, 0.6],
            "event_s": 0.1,
            "baseline_s": [0, 0.09],
            "steady_s": [0.5, 0.6],
            "settling_fraction": 0.02,
            "minimum_hold_s": 0.02,
            "signals": {"reference": "ref", "response": "actual", "saturation": "sat"},
        }
        manifest = base_manifest(name, {"ref": "A", "actual": "A", "sat": "bool"}, [experiment])
        for channel in manifest["channels"].values():
            channel["max_gap_s"] = dt * 1.1
        rows = []
        for index, point in enumerate(time):
            rows.extend(
                [
                    [point, "daq", "ref", 10.0 if point >= 0.1 else 0.0, "A", 1],
                    [point, "daq", "actual", float(values[index]), "A", 1],
                    [point, "daq", "sat", 0, "bool", 1],
                ]
            )
        if mutate:
            mutate(manifest, rows)
        capture = self.directory / f"{name}.csv"
        with capture.open("w", newline="", encoding="utf-8") as stream:
            writer = csv.writer(stream)
            writer.writerow(COLUMNS)
            writer.writerows(rows)
        manifest["captures"] = [
            {
                "path": capture.name,
                "sha256": sha(capture),
                "evidence_class": manifest["evidence_class"],
            }
        ]
        path = self.directory / f"{name}.json"
        writejson(path, manifest)
        return path

    def test_first_order_known_response(self):
        result, _ = analyze(self.write_run())
        metrics = result["runs"][0]["results"][0]["metrics"]
        self.assertAlmostEqual(metrics["rise_10_90_s"], 0.01 * math.log(9), delta=0.00011)
        self.assertAlmostEqual(metrics["settling_s"], 0.01 * math.log(50), delta=0.00011)
        self.assertAlmostEqual(metrics["overshoot_percent"], 0, places=5)
        self.assertAlmostEqual(metrics["steady_error_a"], 0, places=5)

    def test_falling_step(self):
        def change(manifest, rows):
            for row in rows:
                if row[2] in {"ref", "actual"}:
                    row[3] = 10 - row[3]

        result, _ = analyze(self.write_run(mutate=change))
        self.assertAlmostEqual(
            result["runs"][0]["results"][0]["metrics"]["rise_10_90_s"],
            0.01 * math.log(9),
            delta=0.00011,
        )

    def test_undersampled_unresolved(self):
        result, _ = analyze(self.write_run(dt=0.01))
        record = result["runs"][0]["results"][0]
        self.assertIsNone(record["metrics"]["rise_10_90_s"])
        self.assertTrue(any("undersampled" in issue for issue in record["issues"]))

    def test_unreached_response(self):
        result, _ = analyze(self.write_run(tau=10))
        self.assertIsNone(result["runs"][0]["results"][0]["metrics"]["settling_s"])

    def test_noisy_baseline_and_saturation(self):
        def change(manifest, rows):
            for index, row in enumerate(rows):
                if row[2] == "actual" and row[0] < 0.1:
                    row[3] = 0.2 if index % 2 else -0.2
                if row[2] == "sat" and row[0] >= 0.1:
                    row[3] = 1

        result, _ = analyze(self.write_run(mutate=change))
        record = result["runs"][0]["results"][0]
        self.assertIsNone(record["metrics"]["rise_10_90_s"])
        self.assertGreater(record["metrics"]["saturation_time_fraction"], 0.8)

    def test_invalid_sample_is_not_interpolated(self):
        def change(manifest, rows):
            rows[3001][5] = 0

        result, _ = analyze(self.write_run(mutate=change))
        self.assertEqual(result["runs"][0]["results"][0]["status"], "unresolved")

    def test_gap_is_not_interpolated(self):
        def change(manifest, rows):
            rows[:] = [row for row in rows if not 0.11 < row[0] < 0.12]

        result, _ = analyze(self.write_run(mutate=change))
        self.assertEqual(result["runs"][0]["results"][0]["status"], "unresolved")

    def test_unsynchronized_source(self):
        def change(manifest, rows):
            manifest["sources"]["daq"]["synchronized"] = False

        result, _ = analyze(self.write_run(mutate=change))
        self.assertEqual(result["runs"][0]["results"][0]["status"], "unresolved")

    def test_nonfinite_sample_rejected(self):
        for bad in ("nan", "inf", "-inf"):

            def change(manifest, rows):
                rows[1][3] = bad

            with self.assertRaisesRegex(ValueError, "finite"):
                Run(self.write_run(name=f"bad_{bad.replace('-', '_')}", mutate=change))

    def test_timestamp_regression_and_duplicates_rejected(self):
        def change(manifest, rows):
            rows[4][0] = rows[1][0]

        with self.assertRaisesRegex(ValueError, "timestamp"):
            Run(self.write_run(mutate=change))

    def test_hash_mismatch_rejected(self):
        path = self.write_run()
        with (self.directory / "analytic.csv").open("a") as stream:
            stream.write("extra\n")
        with self.assertRaisesRegex(ValueError, "hash"):
            Run(path)

    def test_missing_channels_unresolved(self):
        def change(manifest, rows):
            manifest["experiments"][0]["signals"].pop("response")

        result, _ = analyze(self.write_run(mutate=change))
        self.assertEqual(result["runs"][0]["results"][0]["status"], "unresolved")

    def test_demo_never_measured_conclusion(self):
        comparison = self.result["torque_comparison"]
        self.assertEqual(comparison["matched_pairs"], 5)
        self.assertAlmostEqual(comparison["mean_increase_nm"], 20.0, places=9)
        self.assertFalse(comparison["measured_added_torque_supported"])
        self.assertEqual(self.result["evidence_class"], "synthetic_demo")
        for run in self.result["runs"]:
            for experiment in run["results"]:
                if experiment["type"] == "fault":
                    self.assertIsNone(experiment["metrics"]["measured_latency_within_100ms"])

    def test_repeatability_ci_independent_pairs(self):
        comparison = self.result["torque_comparison"]
        # Hand calculation: five deltas [20,20.1,19.9,20.2,19.8], sample s=sqrt(.025).
        margin = 2.776445105 * math.sqrt(0.025) / math.sqrt(5)
        self.assertAlmostEqual(comparison["repeatability_95_ci_nm"][1], 20 + margin, places=8)
        self.assertEqual(comparison["calibration_bound_nm"], 1.0)
        self.assertAlmostEqual(comparison["conservative_comparison_bound_nm"], 1 + margin, places=8)

    def comparison_with_changes(self, change, count=5):
        from benchkit.analysis import compare_torque

        runs = copy.deepcopy(
            [run for run in self.runs if run.manifest.get("mode") in {"mg2_only", "combined"}]
        )
        runs = [run for run in runs if int(run.manifest["pair_id"].split("_")[-1]) <= count]
        change(runs)
        reports = [analyze_run(run) for run in runs]
        return compare_torque(runs, reports, {})

    def test_inadequate_repetitions(self):
        result = self.comparison_with_changes(lambda runs: None, count=2)
        self.assertEqual(result["status"], "unresolved")
        self.assertFalse(result["measured_added_torque_supported"])

    def test_condition_mismatch(self):
        def change(runs):
            for run in runs:
                if run.manifest["mode"] == "combined":
                    run.traces["pack_voltage"].value[:] = 400

        result = self.comparison_with_changes(change)
        self.assertEqual(result["matched_pairs"], 0)

    def test_missing_calibration(self):
        def change(runs):
            runs[0].manifest["channels"]["output_torque"]["calibration"] = None

        result = self.comparison_with_changes(change)
        self.assertEqual(result["status"], "unresolved")

    def test_zero_calibration_bound_not_accepted(self):
        def change(runs):
            runs[0].manifest["channels"]["output_torque"]["calibration"]["expanded_uncertainty"] = 0

        self.assertEqual(self.comparison_with_changes(change)["status"], "unresolved")

    def test_measured_claim_requires_traceability_and_bound(self):
        # Exercise eligibility rules in memory only; this is not measured evidence.
        def incomplete(runs):
            for run in runs:
                run.manifest["evidence_class"] = "measured"
                run.manifest["hardware"]["controller"] = None

        result = self.comparison_with_changes(incomplete)
        self.assertFalse(result["measured_added_torque_supported"])
        self.assertTrue(any("traceability" in issue for issue in result["issues"]))

        def uncertain(runs):
            for run in runs:
                run.manifest["evidence_class"] = "measured"
                run.manifest["channels"]["output_torque"]["calibration"][
                    "expanded_uncertainty"
                ] = 100

        result = self.comparison_with_changes(uncertain)
        self.assertFalse(result["measured_added_torque_supported"])
        self.assertGreater(result["conservative_comparison_bound_nm"], result["mean_increase_nm"])

    def test_unsteady_voltage_not_hidden_by_equal_means(self):
        def change(runs):
            trace = runs[0].traces["pack_voltage"]
            trace.value[:] = 350 + 20 * np.sin(2 * np.pi * trace.time)

        self.assertEqual(self.comparison_with_changes(change)["status"], "unresolved")

    def test_circulation_not_accepted(self):
        def change(runs):
            runs[0].traces["mg1_current"].value[:] = -20

        self.assertEqual(self.comparison_with_changes(change)["status"], "unresolved")

    def test_power_uses_mean_of_products(self):
        record = next(run for run in self.result["runs"] if run["run_id"] == "combined_1")[
            "results"
        ][0]
        self.assertAlmostEqual(record["metrics"]["power"]["efficiency"], 0.9, places=8)
        run = copy.deepcopy(
            next(run for run in self.runs if run.manifest["run_id"] == "combined_1")
        )
        run.manifest["sources"]["daq"]["synchronized"] = False
        self.assertEqual(analyze_run(run)["results"][0]["status"], "unresolved")

    def test_power_preserves_covariance_and_requires_calibration(self):
        run = copy.deepcopy(
            next(run for run in self.runs if run.manifest["run_id"] == "combined_1")
        )
        time = run.traces["output_torque"].time
        sine = np.sin(2 * np.pi * time)
        run.traces["output_torque"].value[:] = 40 + 10 * sine
        run.traces["output_speed"].value[:] = 1000 + 5 * sine
        run.traces["pack_voltage"].value[:] = 300 + 20 * sine
        run.traces["pack_current"].value[:] = 20 + 5 * sine
        result = power(run, run.manifest["experiments"][0])
        self.assertAlmostEqual(result["electrical_power_w"], 6050, places=6)
        self.assertAlmostEqual(result["mechanical_power_w"], 40025 * 2 * math.pi / 60, places=6)
        self.assertNotAlmostEqual(result["efficiency"], 40000 * 2 * math.pi / 60 / 6000, places=4)
        run.manifest["channels"]["pack_current"]["calibration"] = None
        self.assertIsNone(power(run, run.manifest["experiments"][0])["efficiency"])

    def test_reused_measurement_capture_rejected(self):
        # Fake measured metadata is temporary test input only, never presented as measurements.
        first = loadjson(self.write_run())
        first["evidence_class"] = first["captures"][0]["evidence_class"] = "measured"
        writejson(self.directory / "first.json", first)
        second = copy.deepcopy(first)
        second["run_id"] = "distinct_name_same_capture"
        writejson(self.directory / "second.json", second)
        campaign = self.directory / "campaign.json"
        writejson(campaign, {"schema_version": 1, "runs": ["first.json", "second.json"]})
        with self.assertRaisesRegex(ValueError, "reused"):
            analyze(campaign)

    def test_fault_requires_physical_scope(self):
        run = copy.deepcopy(
            next(run for run in self.runs if run.manifest["run_id"] == "fault_timing")
        )
        self.assertAlmostEqual(
            analyze_run(run)["results"][0]["metrics"]["trigger_to_inhibit_s"], 0.025, delta=0.00011
        )
        run.manifest["sources"]["daq"]["kind"] = "can"
        self.assertEqual(analyze_run(run)["results"][0]["status"], "unresolved")

    def test_current_uncertainty_blocks_decay_claim(self):
        run = copy.deepcopy(
            next(run for run in self.runs if run.manifest["run_id"] == "fault_timing")
        )
        run.manifest["channels"]["motor_current"]["calibration"] = None
        self.assertIsNone(analyze_run(run)["results"][0]["metrics"]["current_decay_s"])

    def test_short_thermal_window_does_not_rate_motor(self):
        run = copy.deepcopy(
            next(run for run in self.runs if run.manifest["run_id"] == "thermal_endurance")
        )
        run.manifest["experiments"][0]["steady_s"] = [1750, 1800]
        result = analyze_run(run)["results"][0]
        self.assertIsNone(
            result["metrics"]["temperatures"]["oil_temp"]["stabilized_by_study_criterion"]
        )
        self.assertFalse(result["metrics"]["continuous_hardware_rating_established"])

    def test_thermal_oscillation_is_not_stabilized(self):
        run = copy.deepcopy(
            next(run for run in self.runs if run.manifest["run_id"] == "thermal_endurance")
        )
        run.traces["oil_temp"].value[:] = 80 + 5 * np.cos(
            2 * np.pi * run.traces["oil_temp"].time / 60
        )
        result = analyze_run(run)["results"][0]
        self.assertFalse(
            result["metrics"]["temperatures"]["oil_temp"]["stabilized_by_study_criterion"]
        )

    def test_mixed_classes_rejected(self):
        runpath = self.write_run()
        manifest = loadjson(runpath)
        manifest["evidence_class"] = "measured"
        with self.assertRaisesRegex(ValueError, "Mixed"):
            validate_manifest(manifest)
        # Temporary fake metadata exercises the loader only; never published as measurements.
        manifest["captures"][0]["evidence_class"] = "measured"
        writejson(runpath, manifest)
        campaign = self.directory / "mixed.json"
        writejson(campaign, {"schema_version": 1, "runs": [str(self.runs[0].path), str(runpath)]})
        with self.assertRaisesRegex(ValueError, "Mixed"):
            analyze(campaign)

    def test_duplicate_run_rejected(self):
        campaign = self.directory / "duplicate.json"
        writejson(campaign, {"schema_version": 1, "runs": [str(self.runs[0].path)] * 5})
        with self.assertRaisesRegex(ValueError, "Duplicate"):
            analyze(campaign)

    def test_import_units_and_raw_preservation(self):
        path = self.write_run()
        template = loadjson(path)
        template["captures"] = []
        template_path = self.directory / "template.json"
        writejson(template_path, template)
        raw = self.directory / "instrument.csv"
        raw.write_text("ms,current_mA,quality\n0,1000,1\n1,2000,1\n2,,0\n", encoding="utf-8")
        mapping_path = self.directory / "mapping.json"
        writejson(
            mapping_path,
            {
                "source": "daq",
                "time_column": "ms",
                "time_unit": "ms",
                "channels": {
                    "current_mA": {"signal": "actual", "unit": "mA", "valid_column": "quality"}
                },
            },
        )
        original = raw.read_bytes()
        imported = import_capture(raw, mapping_path, template_path, self.directory / "converted")
        run = Run(imported)
        self.assertEqual(raw.read_bytes(), original)
        self.assertEqual(run.traces["actual"].value[:2].tolist(), [1, 2])
        self.assertEqual(run.traces["actual"].time.tolist(), [0, 0.001, 0.002])
        self.assertFalse(run.traces["actual"].valid[-1])
        mapping = loadjson(mapping_path)
        mapping["channels"]["current_mA"]["unit"] = "V"
        writejson(mapping_path, mapping)
        with self.assertRaisesRegex(ValueError, "incompatible"):
            import_capture(raw, mapping_path, template_path, self.directory / "bad")

    def test_fixture_physics_and_unknown_ratings(self):
        vehicle = loadjson(ROOT / "configs/capstone_vehicle.json")
        report = size_fixture(vehicle, loadjson(ROOT / "configs/bench-fixture-study.json"))
        point = next(
            p for p in report["points"] if p["speed_mph"] == 70 and p["grade_fraction"] == 0.06
        )
        self.assertAlmostEqual(point["each_output_rpm"], 1056.85, delta=0.1)
        self.assertAlmostEqual(
            point["each_absorber_torque_nm"] * point["each_output_rpm"] * 2 * math.pi / 60,
            point["total_absorption_w"] / 2,
        )
        self.assertFalse(report["equipment_ratings_verified"])
        self.assertIsNone(report["required_peak_each_output_torque_nm_if_supplied"])

    def test_multiple_capture_import_and_raw_hashes(self):
        template = loadjson(self.write_run())
        template["captures"] = []
        template_path = self.directory / "template.json"
        writejson(template_path, template)
        first = self.directory / "first.csv"
        first.write_text("time,current\n0,1\n0.001,2\n", encoding="utf-8")
        second = self.directory / "second.csv"
        second.write_text("time,current\n0.002,3\n0.003,4\n", encoding="utf-8")
        mapping = self.directory / "mapping.json"
        writejson(
            mapping,
            {
                "source": "daq",
                "time_column": "time",
                "time_unit": "s",
                "channels": {"current": {"signal": "actual", "unit": "A"}},
            },
        )
        path = import_captures(
            [first, second], [mapping, mapping], template_path, self.directory / "multi"
        )
        self.assertEqual(Run(path).traces["actual"].value.tolist(), [1, 2, 3, 4])
        (path.parent / "source02/raw/second.csv").write_text("altered", encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "provenance"):
            Run(path)
        with self.assertRaisesRegex(ValueError, "one mapping"):
            import_captures([first, second], [mapping], template_path, self.directory / "bad")

    def test_plan_templates_are_unconfirmed(self):
        create_plan(
            self.directory / "plan",
            ROOT / "configs/capstone_vehicle.json",
            ROOT / "configs/bench-fixture-study.json",
        )
        paths = list((self.directory / "plan/templates").glob("*-manifest.json"))
        self.assertEqual(len(paths), 6)
        for path in paths:
            manifest = validate_manifest(loadjson(path), require_captures=False)
            self.assertFalse(manifest["sources"]["instrument"]["synchronized"])
            self.assertTrue(
                all(channel["calibration"] is None for channel in manifest["channels"].values())
            )

    def test_cli_and_presentation_export(self):
        output = self.directory / "report"
        result = subprocess.run(
            [
                sys.executable,
                str(ROOT / "tools/bench.py"),
                "report",
                "--input",
                str(self.demo),
                "--out",
                str(output),
            ],
            capture_output=True,
            text=True,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        report = (output / "report.md").read_text(encoding="utf-8")
        self.assertIn("SYNTHETIC DEMONSTRATION - NOT MEASURED", report)
        self.assertIn("No measured-performance conclusion", report)
        self.assertTrue((output / "summary.csv").exists())
        self.assertEqual(len(list((output / "figures").glob("*.svg"))), 7)
        from PIL import Image

        for png in (output / "figures").glob("*.png"):
            with Image.open(png) as image:
                self.assertGreaterEqual(image.width, 3000)
                self.assertAlmostEqual(image.info["dpi"][0], 300, delta=0.1)
        invalid = subprocess.run(
            [
                sys.executable,
                str(ROOT / "tools/bench.py"),
                "analyze",
                "--input",
                str(self.directory / "missing.json"),
            ],
            capture_output=True,
            text=True,
        )
        self.assertEqual(invalid.returncode, 2)


if __name__ == "__main__":
    unittest.main()
