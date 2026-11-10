import copy
import json
import math
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from drivetrain_analysis import analyze
from analyze_dyno import compare
from thermal_analysis import thermal_case, thermal_report
from oil_cooling_analysis import oil_case, oil_report, exchanger_conductance
from refine_build import refine, tire_geometry
from leaf_pack_analysis import compare_packs


class AnalysisTests(unittest.TestCase):
    def setUp(self):
        self.config = json.loads((ROOT / "configs/capstone_vehicle.json").read_text())

    def test_kinematics(self):
        self.config["rolling_diameter_m"] = 0.6  # retain the original independently checked example
        a = analyze(self.config)
        n = a["locked_planetary_rpm"]["mg2"]
        self.assertAlmostEqual(n, 4096.81, delta=1)
        self.assertAlmostEqual(a["fixed_carrier_rpm"]["mg1"], -2.6 * n)
        self.assertAlmostEqual(30 * (-2.6 * n) + 78 * n, 0, delta=1e-8)
        self.assertIsNone(a["gates"]["pack_steady_state_power"])
        self.assertIn("measured_continuous_thermal_capability", a["unresolved"])

    def test_roadload_and_voltage(self):
        baseline = analyze(self.config)
        self.config["grade_fraction"] = 0.05
        self.config["pack_min_v"] = 300
        self.config["pack_discharge_limit_a"] = 1
        self.config["mg1_back_emf_line_rms_v_per_rpm"] = 0.05
        a = analyze(self.config)
        self.assertGreater(
            a["road_load_w"]["battery_total"], baseline["road_load_w"]["battery_total"]
        )
        self.assertFalse(a["gates"]["pack_steady_state_power"])
        self.assertGreater(a["no_field_weakening_bus_requirement_v"]["mg1"], 300)

    def test_published_back_emf_proxy(self):
        self.config["rolling_diameter_m"] = 0.6
        a = analyze(self.config)
        self.assertAlmostEqual(a["back_emf_estimates"]["mg2"]["estimated_line_rms_v"], 369, delta=1)
        self.assertAlmostEqual(
            a["back_emf_estimates"]["mg2"]["estimated_line_peak_v"], 580, delta=1
        )
        self.assertGreater(
            a["back_emf_estimates"]["mg2"]["required_effective_flux_reduction_by_bus"]["400"], 0.3
        )

    def test_invalid(self):
        for key, value in (
            ("rolling_diameter_m", 0),
            ("target_mph", math.nan),
            ("drivetrain_efficiency", 2),
            ("pack_min_v", -1),
            ("sun_teeth", True),
        ):
            c = copy.deepcopy(self.config)
            c[key] = value
            with self.assertRaises(ValueError):
                analyze(c)

    def test_thermal_energy_balance_and_cooling(self):
        c = json.loads((ROOT / "configs/civic-radiator-estimate.json").read_text())
        s = c["scenarios"]["nominal_airflow"]
        zero = thermal_case(c, s, 0)
        self.assertAlmostEqual(zero["coolant_hot_c"], c["ambient_c"])
        a = thermal_case(c, s, 20000)
        self.assertAlmostEqual(a["total_heat_w"], a["traction_dc_w"] - a["wheel_w"])
        better = copy.deepcopy(s)
        better["radiator_ua_w_k"] *= 2
        b = thermal_case(c, better, 20000)
        self.assertLess(b["coolant_hot_c"], a["coolant_hot_c"])
        self.assertLess(
            b["motors"]["mg2"]["steady_winding_c"], a["motors"]["mg2"]["steady_winding_c"]
        )
        worse = copy.deepcopy(s)
        worse["mg2_winding_to_coolant_k_w"] *= 2
        self.assertGreater(
            thermal_case(c, worse, 20000)["motors"]["mg2"]["steady_winding_c"],
            a["motors"]["mg2"]["steady_winding_c"],
        )
        report = thermal_report(self.config, c)
        self.assertLess(
            report["scenarios"]["pessimistic_low_airflow"][
                "thermal_only_shaft_ceiling_kw_not_power_rating"
            ],
            report["scenarios"]["nominal_airflow"][
                "thermal_only_shaft_ceiling_kw_not_power_rating"
            ],
        )
        with self.assertRaises(ValueError):
            thermal_case(c, s, math.nan)

    def samples(self):
        return [
            dict(
                mode=mode,
                output_rpm=1000,
                mg2_command_percent=30,
                output_torque_nm=torque,
                torque_uncertainty_nm=2,
                mg1_dc_a=10 if mode == "combined" else 0,
                mg2_dc_a=20,
            )
            for mode, torque in (("mg2_only", 100), ("combined", 120))
            for _ in range(10)
        ]

    def test_oil_network_and_cooler(self):
        cooling = json.loads((ROOT / "configs/civic-radiator-estimate.json").read_text())
        oil = json.loads((ROOT / "configs/transaxle-oil-estimate.json").read_text())
        s = cooling["scenarios"]["nominal_airflow"]
        stock = oil_case(cooling, s, oil, oil["options"]["stock_paths_only"], 20000)
        cooler = oil_case(cooling, s, oil, oil["options"]["candidate_oil_to_coolant_cooler"], 20000)
        for result in (stock, cooler):
            self.assertAlmostEqual(result["oil_node_balance_error_w"], 0, delta=1e-8)
            self.assertAlmostEqual(result["system_heat_balance_error_w"], 0, delta=1e-8)
        self.assertLess(cooler["bulk_oil_c"], stock["bulk_oil_c"])
        self.assertLess(cooler["peak_winding_c"], stock["peak_winding_c"])
        zero = oil_case(cooling, s, oil, oil["options"]["stock_paths_only"], 0)
        self.assertAlmostEqual(zero["bulk_oil_c"], cooling["ambient_c"])
        self.assertEqual(exchanger_conductance(100, 0, 500), 0)
        self.assertLessEqual(exchanger_conductance(1000, 10, 500), 10)
        self.assertAlmostEqual(exchanger_conductance(100, 100, 100), 50)
        control = oil_report(self.config, cooling, oil)["scenarios"]["nominal_airflow"][
            "candidate_oil_to_coolant_cooler"
        ]["thermostatic_control_screen"]
        self.assertTrue(control["target_bracketed_by_available_cooling"])
        self.assertAlmostEqual(control["predicted_oil_c"], 80, delta=0.001)
        self.assertGreater(control["effective_conductance_fraction"], 0)
        self.assertLess(control["effective_conductance_fraction"], 1)
        with self.assertRaises(ValueError):
            exchanger_conductance(math.nan, 10, 500)

    def test_selected_tire_and_leaf_architectures(self):
        self.assertAlmostEqual(tire_geometry("205/45R15")["unloaded_geometric_diameter_m"], 0.5655)
        study = json.loads((ROOT / "configs/leaf-pack-study.json").read_text())
        report = compare_packs(self.config, study)
        original, reduced, low = report["configurations"]
        self.assertEqual(original["nominal_v"], 360)
        self.assertAlmostEqual(original["full_v"], 398.4)
        self.assertAlmostEqual(original["example_operating_energy_kwh"], 13.44)
        self.assertAlmostEqual(
            low["example_operating_energy_kwh"], original["example_operating_energy_kwh"]
        )
        self.assertAlmostEqual(
            low["loads"][0]["current_at_assumed_loaded_floor_a"],
            2 * original["loads"][0]["current_at_assumed_loaded_floor_a"],
        )
        self.assertGreater(
            low["required_effective_flux_reduction_at_loaded_floor"],
            original["required_effective_flux_reduction_at_loaded_floor"],
        )
        self.assertEqual(reduced["early_modules_used"], 36)
        self.assertGreater(report["motor_rpm_at_target"], 4300)

    def test_tire_and_build_refinement(self):
        inputs = json.loads((ROOT / "configs/build-inputs.json").read_text())
        cooling = json.loads((ROOT / "configs/civic-radiator-estimate.json").read_text())
        oil = json.loads((ROOT / "configs/transaxle-oil-estimate.json").read_text())
        geometry = tire_geometry("185/65R14")
        self.assertAlmostEqual(geometry["unloaded_geometric_diameter_m"], 0.5961)
        with self.assertRaises(ValueError):
            tire_geometry("garbage")
        before = copy.deepcopy(self.config)
        inputs["tires"]["sidewall_code"] = "185/65R14"
        inputs["tires"]["measured_loaded_rolling_circumference_m"] = 1.8
        report = refine(self.config, inputs, cooling, oil)
        self.assertAlmostEqual(
            report["effective_vehicle_model"]["rolling_diameter_m"], 1.8 / math.pi
        )
        self.assertEqual(self.config, before)
        self.assertEqual([x["grade_percent"] for x in report["grade_sweep"]], [0, 3, 6])
        self.assertGreater(
            report["grade_sweep"][2]["estimated_battery_kw"],
            report["grade_sweep"][0]["estimated_battery_kw"],
        )
        self.assertIn("battery.minimum_loaded_v", report["missing_inputs"])
        inputs["battery"]["minimum_loaded_v"] = 400
        inputs["battery"]["maximum_charged_v"] = 300
        with self.assertRaises(ValueError):
            refine(self.config, inputs, cooling, oil)

    def test_measured_comparison(self):
        rows = self.samples()
        a = compare(rows)
        self.assertTrue(a["increase_exceeds_bound"])
        self.assertEqual(a["torque_increase_nm"], 20)
        self.assertEqual(a["combined_comparison_bound_nm"], 4)
        for r in rows:
            if r["mode"] == "combined":
                r["output_torque_nm"] = 103
        self.assertFalse(compare(rows)["increase_exceeds_bound"])

    def test_bad_experiment(self):
        for key, value in (
            ("output_rpm", 1100),
            ("mg2_command_percent", 40),
            ("mg1_dc_a", -10),
            ("torque_uncertainty_nm", 0),
        ):
            rows = self.samples()
            for r in rows:
                if r["mode"] == "combined":
                    r[key] = value
            with self.assertRaises(ValueError):
                compare(rows)
        with self.assertRaises(ValueError):
            compare([])


if __name__ == "__main__":
    unittest.main()
