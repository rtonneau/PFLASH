"""Unit tests of the pure parts of the dose-rate study tooling.

Run: python -m unittest discover -s examples/dose-rate-study/studies -v
"""
import json
import math
import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import analyze  # noqa: E402
import make_configs  # noqa: E402
import merge_libraries  # noqa: E402
import spike_report  # noqa: E402


class MakeConfigsTest(unittest.TestCase):
    def study(self, **kw):
        s = dict(make_configs.DEFAULTS)
        s.update({"rates": [0.1, 1e4], "realizations": {"0.1": 2, "10000": 3}})
        s.update(kw)
        with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as handle:
            json.dump(s, handle)
        try:
            return make_configs.load_study(handle.name)
        finally:
            os.unlink(handle.name)

    def test_target_energy_of_1_gy_in_4_um_box(self):
        self.assertAlmostEqual(make_configs.box_mass_kg(2.0), 6.4e-14, delta=1e-20)
        self.assertAlmostEqual(make_configs.target_energy_ev(1.0, 2.0), 3.99457e5, delta=1.0)

    def test_realization_keys_compare_as_numbers_and_override(self):
        s = self.study()
        self.assertEqual(s["realizations"], {"0.1": 2, "10000": 3})
        with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as handle:
            json.dump({"rates": [1e4], "realizations": {"1e4": 1}}, handle)
        try:
            s2 = make_configs.load_study(handle.name, {"10000": 7})
        finally:
            os.unlink(handle.name)
        self.assertEqual(s2["realizations"], {"10000": 7})

    def test_missing_realization_count_is_an_error(self):
        with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as handle:
            json.dump({"rates": [1, 2], "realizations": {"1": 1}}, handle)
        try:
            with self.assertRaises(ValueError):
                make_configs.load_study(handle.name)
        finally:
            os.unlink(handle.name)

    def test_feasible_offsets_hold_target_plus_one_track(self):
        offsets = make_configs.feasible_offsets([10, 10, 10, 10, 10], 25)   # need 35
        self.assertEqual(list(offsets), [0, 1])

    def test_runs_have_distinct_seeds_and_feasible_offsets(self):
        s = self.study(doseGy=1e-9)
        deposits = [600.0] * 100
        runs = make_configs.build_runs(s, deposits)
        self.assertEqual(len(runs), 5)
        self.assertEqual(len({r["chemistrySeed"] for r in runs}), 5)
        self.assertEqual(len({r["sourceSeed"] for r in runs}), 5)
        self.assertEqual(len({r["id"] for r in runs}), 5)
        feasible = set(make_configs.feasible_offsets(deposits, make_configs.target_energy_ev(1e-9, 2.0)).tolist())
        self.assertTrue(all(r["firstPacket"] in feasible for r in runs))
        self.assertEqual(runs, make_configs.build_runs(s, deposits))   # reproducible

    def test_too_small_library_is_an_error(self):
        with self.assertRaises(ValueError):
            make_configs.build_runs(self.study(), [600.0] * 10)

    def test_config_text(self):
        s = self.study()
        run = {"chemistrySeed": 5, "sourceSeed": 6, "rate_Gy_s": 50.0, "firstPacket": 3, "snapshots": True}
        text = make_configs.config_text(s, run, "../lib")
        for line in ("mode = doseCutoff", "libraryDir = ../lib", "doseRateGyPerS = 50.0", "firstPacket = 3",
                     "steadyStateWindow = 50", "steadyStateTolerance = 0.05", "snapshots = true", "halfBoxUm = 2.0"):
            self.assertIn(line + "\n", text)


class AnalyzeTest(unittest.TestCase):
    def test_grid_index(self):
        self.assertEqual(analyze.grid_index(1e-3), 0)
        self.assertEqual(analyze.grid_index(1.0), 30)
        self.assertEqual(analyze.grid_index(analyze.grid_time(57)), 57)
        self.assertIsNone(analyze.grid_index(2.0))

    def test_ratio_estimate(self):
        g, se = analyze.ratio_estimate([2.0, 4.0], [100.0, 100.0])
        self.assertAlmostEqual(g, 3.0)
        # residuals -1, +1 -> var = 2 / 2 = 1 -> SE = 100 * 1 / 100
        self.assertAlmostEqual(se, 1.0)
        g1, se1 = analyze.ratio_estimate([2.0], [100.0])
        self.assertAlmostEqual(g1, 2.0)
        self.assertTrue(math.isnan(se1))
        self.assertTrue(all(math.isnan(v) for v in analyze.ratio_estimate([1.0], [0.0])))

    def run_data(self, n_at_1ns, e_at_1ns, final_e, extrapolated, beam_off_ns):
        grid = {30: {"A": {"N": n_at_1ns, "E": e_at_1ns, "dose": 0.0, "x": extrapolated}}}
        off = {"A": {"N": 1.0, "E": final_e, "dose": 1.0, "x": 0}}
        return grid, off, beam_off_ns

    def test_aggregate_both_normalisations_and_extrapolation_flag(self):
        runs = [self.run_data(10, 100, 1000, 0, 5.0), self.run_data(30, 100, 1000, 1, 7.0)]
        time_rows, off_rows = analyze.aggregate(runs)
        self.assertEqual(len(time_rows), 1)
        row = time_rows[0]
        self.assertAlmostEqual(row["time_ns"], 1.0)
        self.assertAlmostEqual(row["G_cum"], 20.0)
        self.assertAlmostEqual(row["G_final"], 2.0)
        self.assertEqual(row["n_realizations"], 2)
        self.assertEqual(row["extrapolated"], 1)
        self.assertAlmostEqual(row["extrapolatedFraction"], 0.5)
        self.assertAlmostEqual(off_rows[0]["time_ns"], 6.0)
        self.assertAlmostEqual(off_rows[0]["mean_dose_Gy"], 1.0)

    def test_read_species_separates_beam_off(self):
        with tempfile.TemporaryDirectory() as d:
            path = os.path.join(d, "species.csv")
            with open(path, "w", encoding="utf-8") as handle:
                handle.write("timeNs,species,count,energyDepositEv,doseGy,gCumulativePer100eV,gFinalPer100eV,extrapolated\n"
                             "1,A,5,100,0.1,5,1,0\n10,A,6,200,0.2,3,1,0\n12.5,A,7,300,0.3,2,2,0\n")
            grid, off, t = analyze.read_species(path)
        self.assertEqual(sorted(grid), [30, 40])
        self.assertEqual(t, 12.5)
        self.assertEqual(off["A"]["N"], 7.0)


class MergeTest(unittest.TestCase):
    def part(self, seeds, skipped, deposit=100.0):
        header = ["packetIndex", "file", "physicsSeed", "energyDepositEv", "moleculeCount"]
        rows = [{"packetIndex": str(i), "file": "packet-%05d.txt" % i, "physicsSeed": str(s),
                 "energyDepositEv": str(deposit), "moleculeCount": "3"} for i, s in enumerate(seeds)]
        summary = {"compatibilityTag": "t", "captureHalfSide_nm": 2000.0, "worldHalfSide_nm": 1e4,
                   "packets": [{"packetIndex": i, "file": r["file"]} for i, r in enumerate(rows)],
                   "skippedEmptyEvents": [{"physicsSeed": s, "energyDeposit_eV": 0} for s in skipped],
                   "totals": {"packetCount": len(rows), "energyDeposit_eV": deposit * len(rows)}}
        return header, rows, summary

    def test_renumbers_and_sums(self):
        _, rows, merged, stats = merge_libraries.merge_summaries([self.part([1, 2], [3]), self.part([10], [])])
        self.assertEqual([r["packetIndex"] for r in rows], ["0", "1", "2"])
        self.assertEqual([r["file"] for r in rows], ["packet-00000.txt", "packet-00001.txt", "packet-00002.txt"])
        self.assertEqual(rows[2]["_source"], "packet-00000.txt")
        self.assertEqual(merged["totals"]["packetCount"], 3)
        self.assertEqual(len(merged["skippedEmptyEvents"]), 1)
        self.assertAlmostEqual(stats["hitFraction"], 0.75)
        self.assertAlmostEqual(stats["meanDepositPerLaunched_eV"], 75.0)

    def test_duplicate_seed_and_mismatch_are_errors(self):
        with self.assertRaises(ValueError):
            merge_libraries.merge_summaries([self.part([1], []), self.part([1], [])])
        other = self.part([5], [])
        other[2]["captureHalfSide_nm"] = 1000.0
        with self.assertRaises(ValueError):
            merge_libraries.merge_summaries([self.part([1], []), other])


class SpikeReportTest(unittest.TestCase):
    LINES = ["HISTORY_PROGRESS wall_s=%d simTime_ns=%g endpoint_ns=1e9 activated=%d/100 liveMolecules=%d steps=1"
             % (w, w * 10.0, a, 50 + a) for w, a in [(30, 10), (60, 20), (90, 25), (120, 30)]]

    def test_parse_progress(self):
        p = spike_report.parse_progress(["noise"] + self.LINES)
        self.assertEqual(len(p), 4)
        self.assertEqual(p[-1], (120.0, 1200.0, 1e9, 30, 100, 80))

    def test_estimate_full_wall_uses_recent_rate(self):
        p = spike_report.parse_progress(self.LINES)
        # tail = last 2 lines: 5 arrivals in 30 s; 70 remaining -> +420 s
        self.assertAlmostEqual(spike_report.estimate_full_wall(130.0, p), 130.0 + 420.0)
        self.assertIsNone(spike_report.estimate_full_wall(10.0, []))


if __name__ == "__main__":
    unittest.main()
