// Fixture 3, rebound fold (patch rebound-fold.patch, geant4-issues.md issue 3).
//
// G4ChemReboundTransportation::calculateNextCoordinate reflects a molecule's
// next coordinate into the box [low, high]. The dose-rate study runs every
// molecule except H2O in a reflecting 4 um box with this transport, and its
// late IRT_syn steps are long: a 1 ms step of e_aq has a 1D rms displacement of
// sqrt(2 D dt) = 3.1 um, so steps overshooting a wall by more than the box are
// common. The stock fold is wrong for those and for points on the far wall.
//
// Part A: the fixed table of issue 3. Part B: Gaussian steps from uniform
// positions in a 4 um box; count results outside the box and compare with an
// independent reference fold (period 2L).
#include "PatchCheck.hh"

#include "G4ChemReboundTransportation.hh"
#include "G4SystemOfUnits.hh"

#include <cmath>
#include <random>

namespace
{
double ReferenceFold(double x, double high, double low)
{
  const double length = high - low;
  double f = std::fmod(x - low, 2. * length);
  if (f < 0.) f += 2. * length;
  return low + (f <= length ? f : 2. * length - f);
}
}  // namespace

int main(int argc, char** argv)
{
  const auto expect = PatchCheck::ParseExpect(argc, argv);
  PatchCheck::Checker c("rebound-fold", expect);
  const bool fixed = expect == PatchCheck::Expect::Fixed;
  c.Module("G4processes_core", "G4processes_core.dll");

  // Part A: the table of issue 3 on [0, 1], and the 4 um box case.
  struct Row { const char* key; double raw, high, low, stock, correct, unit; };
  const Row rows[] = {
    {"unit_2.1", 2.1, 1., 0., 1.7, 0.1, 1.},
    {"unit_2.9", 2.9, 1., 0., -0.7, 0.9, 1.},
    {"unit_1.0", 1.0, 1., 0., 0.0, 1.0, 1.},
    {"box_7um_in_um", 7. * um, 2. * um, -2. * um, 3. * um, -1. * um, um},
  };
  for (const auto& r : rows) {
    const double got = G4ChemReboundTransportation::calculateNextCoordinate(r.raw, r.high, r.low);
    const double scale = (r.high - r.low);
    c.Result(r.key, got / r.unit);
    const double expected = fixed ? r.correct : r.stock;
    c.Check(PatchCheck::Checker::Near(got, expected, 1e-9 * scale),
            std::string(r.key) + (fixed ? " folds correctly" : " reproduces the stock value"));
  }

  // Part B: dose-rate scenario, 4 um reflecting box, 1 ms e_aq steps.
  const double low = -2. * um, high = 2. * um;
  const double diffusion = 4.9e-9 * m2 / s;  // e_aq
  const double dt = 1. * ms;
  const double sigma = std::sqrt(2. * diffusion * dt);
  std::mt19937_64 rng(20261117);
  std::uniform_real_distribution<double> start(low, high);
  std::normal_distribution<double> step(0., sigma);
  const int samples = 100000;
  int outside = 0, mismatch = 0, overshoot = 0;
  for (int i = 0; i < samples; ++i) {
    const double raw = start(rng) + step(rng);
    if (raw > high + (high - low) || raw < low - (high - low)) ++overshoot;
    const double got = G4ChemReboundTransportation::calculateNextCoordinate(raw, high, low);
    if (got < low || got > high) ++outside;
    if (!PatchCheck::Checker::Near(got, ReferenceFold(raw, high, low), 1e-9 * um)) ++mismatch;
  }
  c.Result("steps", samples);
  c.Result("sigma_um", sigma / um);
  c.Result("overshootMoreThanBox", overshoot);
  c.Result("outsideBox", outside);
  c.Result("mismatchReference", mismatch);
  c.Check(overshoot > 0, "the scenario contains steps overshooting by more than the box");
  if (fixed) {
    c.Check(outside == 0, "no coordinate lands outside the box");
    c.Check(mismatch == 0, "every coordinate equals the reference fold");
  }
  else {
    c.Check(outside > 0, "some coordinates land outside the box");
    c.Check(mismatch > 0, "some coordinates differ from the reference fold");
  }
  return c.Finish();
}
