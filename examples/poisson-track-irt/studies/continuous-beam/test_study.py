"""Unit tests of the dose-rate study tools, on synthetic data with known answers."""
import csv
import json
import math
import os
import tempfile
import unittest

import numpy as np

import analyze
import make_schedules as ms


def write_library(directory, n, e_dep=100000.0):
    os.makedirs(directory, exist_ok=True)
    with open(os.path.join(directory, "library.csv"), "w", newline="", encoding="utf-8") as handle:
        w = csv.writer(handle)
        w.writerow(["packetIndex", "file", "physicsSeed", "primaryParticle", "primaryEnergyEv",
                    "energyDepositEv", "escapedEnergyEv", "energyResidualEv", "moleculeCount"])
        for i in range(n):
            w.writerow([i, "packet-%05d.txt" % i, 100 + i, "e-", 100000.0, e_dep, 0, 0, 10])
    packets = [{"packetIndex": i, "primaryOrigin_nm": [10.0 * i, -5.0 * i, -2499.0]} for i in range(n)]
    with open(os.path.join(directory, "capture.json"), "w", encoding="utf-8") as handle:
        json.dump({"packets": packets}, handle)


def read_text(path):
    with open(path, encoding="utf-8") as handle:
        return handle.read()


def read_json(path):
    return json.loads(read_text(path))


def make_study(**overrides):
    study = dict(ms.DEFAULTS)
    study.update({"rates": [100.0, 1000.0], "realizations": {"100": 200, "1000": 200},
                  "sourceSeed": 7})
    study.update(overrides)
    path = tempfile.mktemp(suffix=".json")
    with open(path, "w", encoding="utf-8") as handle:
        json.dump(study, handle)
    try:
        return ms.load_study(path)
    finally:
        os.remove(path)


class PoissonPlanTest(unittest.TestCase):
    def test_rate_formula(self):
        # 100 keV in a 5 um water box: m = 1.25e-13 kg; 1 Gy/s -> 1.25e-13 J/s / 1.602e-14 J
        mass = ms.box_mass_kg(5.0)
        self.assertAlmostEqual(mass, 1.25e-13, delta=1e-25)
        lam = ms.poisson_rate_hz(1.0, mass, 100000.0)
        self.assertAlmostEqual(lam, 1.25e-13 / (1e5 * 1.602176634e-19), delta=1e-9 * lam)
        self.assertAlmostEqual(lam, 7.8, delta=0.05)

    def test_mean_count_matches_rate(self):
        study = make_study(rates=[1000.0], realizations={"1000": 4000})
        plan = ms.build_plan(study, 10 ** 7, 100000.0)
        counts = [r["nPackets"] for r in plan["runs"]] + [0] * plan["empty"]["1000"]
        self.assertEqual(len(counts), 4000)
        expected = ms.poisson_rate_hz(1000.0, ms.box_mass_kg(5.0), 1e5) * 1e-3   # per 1 ms window
        mean = np.mean(counts)
        self.assertLess(abs(mean - expected), 5 * math.sqrt(expected / 4000))
        # Poisson: variance equals the mean
        self.assertLess(abs(np.var(counts) / expected - 1), 0.15)

    def test_times_inside_window_sorted(self):
        plan = ms.build_plan(make_study(), 10 ** 7, 100000.0)
        for run in plan["runs"]:
            t = run["timesNs"]
            self.assertEqual(t, sorted(t))
            self.assertTrue(all(0.0 <= x < 1e6 for x in t))

    def test_disjoint_contiguous_slices(self):
        plan = ms.build_plan(make_study(), 10 ** 7, 100000.0)
        used = []
        for run in plan["runs"]:
            used.extend(range(run["packetStart"], run["packetStart"] + run["nPackets"]))
        self.assertEqual(len(used), len(set(used)))
        self.assertEqual(sorted(used), list(range(plan["packetsUsed"])))

    def test_library_too_small(self):
        study = make_study()
        with self.assertRaisesRegex(ValueError, "library is too small"):
            ms.build_plan(study, 3, 100000.0)

    def test_empty_draws_counted_not_run(self):
        study = make_study(rates=[0.1], realizations={"0.1": 500})
        plan = ms.build_plan(study, 10 ** 7, 100000.0)
        n_run = len(plan["runs"])
        self.assertEqual(n_run + plan["empty"]["0.1"], 500)
        self.assertGreater(plan["empty"]["0.1"], 400)          # lambda*window ~ 0.0078
        self.assertTrue(all(r["nPackets"] >= 1 for r in plan["runs"]))

    def test_reproducible_and_distinct_seeds(self):
        a = ms.build_plan(make_study(), 10 ** 7, 100000.0)
        b = ms.build_plan(make_study(), 10 ** 7, 100000.0)
        self.assertEqual(a["runs"], b["runs"])
        seeds = [r["chemistrySeed"] for r in a["runs"]]
        self.assertEqual(len(seeds), len(set(seeds)))
        c = ms.build_plan(make_study(sourceSeed=8), 10 ** 7, 100000.0)
        self.assertNotEqual(a["runs"], c["runs"])

    def test_files_written(self):
        with tempfile.TemporaryDirectory() as tmp:
            lib = os.path.join(tmp, "lib")
            write_library(lib, 400)
            study_path = os.path.join(tmp, "study.json")
            with open(study_path, "w", encoding="utf-8") as handle:
                json.dump({"rates": [1000], "realizations": {"1000": 5}, "sourceSeed": 3}, handle)
            out = os.path.join(tmp, "plan")
            self.assertEqual(ms.main(["--library", lib, "--study", study_path, "--out", out]), 0)
            plan = read_json(os.path.join(out, "plan.json"))
            self.assertEqual(len(plan["runs"]) + plan["emptyRealizations"]["1000"], 5)
            run = plan["runs"][0]
            cfg = read_text(os.path.join(out, run["config"]))
            for key in ("mode = deterministic", "halfBoxUm = 2.5", "endpointNs = 1000000.0",
                        "chemistrySeed = %d" % run["chemistrySeed"]):
                self.assertIn(key, cfg)
            rows = [l.split() for l in read_text(os.path.join(out, run["schedule"])).splitlines()
                    if not l.startswith("#")]
            self.assertEqual(len(rows), run["nPackets"])
            for row in rows:     # offset = capture origin of the packet, nm -> um
                i = int(row[1])
                self.assertEqual([float(v) for v in row[2:]], [0.01 * i, -0.005 * i, -2.499])
            times = [float(r[0]) for r in rows]
            self.assertEqual(times, sorted(times))
            # refuses a non-empty output directory
            self.assertEqual(ms.main(["--library", lib, "--study", study_path, "--out", out]), 2)


class RatioEstimatorTest(unittest.TestCase):
    def test_known_answer(self):
        y = np.array([[10.0], [20.0]])
        x = np.array([[100.0], [100.0]])
        g, se = analyze.ratio_estimator(y, x)
        # R = 30/200 = 0.15 -> 15 per 100 eV; residuals -5, +5; var = 50/(2*1) = 25; SE = 5/100 -> 5
        self.assertAlmostEqual(g[0], 15.0)
        self.assertAlmostEqual(se[0], 5.0)

    def test_unequal_energies(self):
        y = np.array([[3.0], [8.0], [4.0]])
        x = np.array([[10.0], [30.0], [20.0]])
        g, se = analyze.ratio_estimator(y, x)
        r = 15.0 / 60.0
        resid = y[:, 0] - r * x[:, 0]
        expected = math.sqrt((resid ** 2).sum() / (3 * 2)) / 20.0
        self.assertAlmostEqual(g[0], r * 100)
        self.assertAlmostEqual(se[0], expected * 100)

    def test_proportional_data_has_zero_error(self):
        y = np.array([[2.0, 5.0], [4.0, 10.0], [6.0, 15.0]])
        x = np.array([[10.0, 10.0], [20.0, 20.0], [30.0, 30.0]])
        g, se = analyze.ratio_estimator(y, x)
        np.testing.assert_allclose(g, [20.0, 50.0])
        np.testing.assert_allclose(se, [0.0, 0.0], atol=1e-12)

    def test_single_realization_and_zero_energy(self):
        g, se = analyze.ratio_estimator(np.array([[4.0, 0.0]]), np.array([[20.0, 0.0]]))
        self.assertAlmostEqual(g[0], 20.0)
        self.assertTrue(math.isnan(se[0]))
        self.assertTrue(math.isnan(g[1]) and math.isnan(se[1]))
        g, se = analyze.ratio_estimator(np.array([[1.0], [1.0]]), np.array([[0.0], [0.0]]))
        self.assertTrue(math.isnan(g[0]) and math.isnan(se[0]))

    def test_energy_by_activation(self):
        e = analyze.deposited_energy(np.array([1.0, 10.0, 100.0]), np.array([0.001, 50.001]),
                                     np.array([5.0, 7.0]))
        np.testing.assert_allclose(e, [5.0, 5.0, 12.0])

    def test_anchor0_grid(self):
        grid = analyze.anchor0_grid(1.0, 2, 1000.0)
        np.testing.assert_allclose(grid, [1, 10 ** 0.5, 10, 10 ** 1.5, 100, 10 ** 2.5, 1000.0])


def write_run(directory, rows, activations, passed=True):
    """rows: {time: {species: count}}; activations: [(expectedActivationTimeNs, energyDepositEv)]."""
    os.makedirs(directory)
    with open(os.path.join(directory, "species.csv"), "w", newline="", encoding="utf-8") as handle:
        w = csv.writer(handle)
        w.writerow(["timeNs", "species", "count", "history", "chemistrySeed"])
        for t in sorted(rows):
            for s, c in rows[t].items():
                w.writerow([repr(t), s, c, "deterministic", 1])
    with open(os.path.join(directory, "activations.csv"), "w", newline="", encoding="utf-8") as handle:
        w = csv.writer(handle)
        w.writerow(["sourceIndex", "expectedActivationTimeNs", "energyDepositEv"])
        for i, (t, e) in enumerate(activations):
            w.writerow([i, repr(t), repr(e)])
    with open(os.path.join(directory, "ledger.json"), "w", encoding="utf-8") as handle:
        json.dump({"checks": {"allPassed": passed}}, handle)


class AnalyzePipelineTest(unittest.TestCase):
    def test_end_to_end_synthetic(self):
        species = "°OH^0"
        with tempfile.TemporaryDirectory() as tmp:
            plan = {"gridMinAgeNs": 1.0, "gridPointsPerDecade": 1, "endpointNs": 100.0,
                    "boxMass_kg": 1.25e-13, "rates": [1.0, 10.0],
                    "realizations": {"1": 2, "10": 3}, "emptyRealizations": {"1": 1, "10": 1},
                    "runs": []}
            # grid: 1, 10, 100.  rate 1: one run (the other realization was empty).
            # rate 10: two runs. The second run lacks the grid time 10 (near an arrival).
            specs = [
                ("a", 1.0, [(0.001, 100.0)], {1.0: 10, 10.0: 8, 100.0: 6}),
                ("b", 10.0, [(0.001, 100.0)], {1.0: 20, 10.0: 16, 100.0: 12}),
                ("c", 10.0, [(0.001, 100.0), (50.001, 100.0)], {1.0: 10, 100.0: 40}),
            ]
            for name, rate, acts, counts in specs:
                rows = {t: {species: c, "H^0": 1} for t, c in counts.items()}
                write_run(os.path.join(tmp, name), rows, acts)
                plan["runs"].append({"id": name, "rate_Gy_s": rate, "output": name})
            time_rows, end_rows = analyze.analyze(plan, tmp, species=[species])
            by = {(r["rate_Gy_s"], r["time_ns"]): r for r in time_rows}
            self.assertAlmostEqual(by[(1.0, 100.0)]["G_per_100eV"], 6.0)     # 6 / 100 eV *100
            self.assertTrue(math.isnan(by[(1.0, 100.0)]["SE"]))              # n = 1
            self.assertNotIn((10.0, 10.0), by)                               # time absent in run c
            self.assertAlmostEqual(by[(10.0, 1.0)]["G_per_100eV"], 30.0 / 200.0 * 100)
            # time 100: y = 12, 40; x = 100, 200; R = 52/300
            self.assertAlmostEqual(by[(10.0, 100.0)]["G_per_100eV"], 52.0 / 300.0 * 100)
            end = {r["rate_Gy_s"]: r for r in end_rows}
            self.assertEqual(end[10.0]["n_realizations"], 2)
            self.assertEqual(end[10.0]["n_empty"], 1)
            self.assertAlmostEqual(end[10.0]["delta_vs_ref"], 52.0 / 3.0 - 6.0)
            self.assertTrue(math.isnan(end[10.0]["delta_SE"]))               # ref SE is NaN (n = 1)
            self.assertEqual(end[1.0]["delta_vs_ref"], 0.0)
            # mean tracks over all 3 realizations of rate 10 (one empty): (1 + 2 + 0)/3
            self.assertAlmostEqual(end[10.0]["mean_tracks"], 1.0)
            self.assertAlmostEqual(end[10.0]["mean_dose_Gy"],
                                   300.0 * 1.602176634e-19 / 1.25e-13 / 3)

    def test_failed_check_is_an_error(self):
        with tempfile.TemporaryDirectory() as tmp:
            write_run(os.path.join(tmp, "a"), {1.0: {"H^0": 1}}, [(0.001, 1.0)], passed=False)
            plan = {"gridMinAgeNs": 1.0, "gridPointsPerDecade": 1, "endpointNs": 10.0, "boxMass_kg": 1.0,
                    "rates": [1.0], "realizations": {"1": 1}, "emptyRealizations": {"1": 0},
                    "runs": [{"id": "a", "rate_Gy_s": 1.0, "output": "a"}]}
            with self.assertRaises(RuntimeError):
                analyze.analyze(plan, tmp, species=["H^0"])


if __name__ == "__main__":
    unittest.main()
