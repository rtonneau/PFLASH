// Fixture 2, new-old contact at a source activation (patch
// source-contacts-experimental.patch, geant4-issues.md, timed injection).
//
// IRT_syn treats a pair already inside its effective reaction radius as an
// immediate contact (reaction with probability P at once) only when the global
// time equals the scheduler start time. A molecule injected later as a delayed
// track (a new dose-rate source) that lands inside the radius of an older one
// is therefore never given that contact on the stock kernel. The patch adds a
// per-thread source registry (RegisterSourceActivation) and grants the contact
// to pairs with a fresh source member.
//
// Each trial is one standalone chemistry history (own process, own seed):
//   old e_aq at 1 ps (start time) at the origin, kept as the start-time control
//   together with an old-old pair 1 um away, both inside the radius at 1 ps;
//   new molecule at t_a = 1.001 ps, 0.01 nm from the old e_aq.
// The parent runs N trials and counts new-old reactions at exactly t_a.
// Stock: 0. Patched: binomial(N, P), P the pair's reaction probability; the
// old-old start-time contact reacts in every trial on both kernels.
#include "PatchCheck.hh"

#include "G4DNAChemistryManager.hh"
#include "G4DNAMolecularReactionTable.hh"
#include "G4ITTrackHolder.hh"
#include "G4MolecularConfiguration.hh"
#include "G4Molecule.hh"
#include "G4RunManagerFactory.hh"
#include "G4Scheduler.hh"
#include "G4SystemOfUnits.hh"
#include "G4UImanager.hh"
#include "G4UserTimeStepAction.hh"
#include "Randomize.hh"
#include "actions/ActionInitialization.hh"
#include "geometry/DetectorConstruction.hh"
#include "physics/PhysicsList.hh"
#include "scoring/OutputFormatsMessenger.hh"

#if HAS_SOURCE_ACTIVATION
#  include "G4DNAIndependentReactionTimeStepper.hh"
#endif

#include <cmath>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>

namespace
{
constexpr int kOld = 1000002, kNew = 1000003, kOldA = 1000004, kOldB = 1000005;
const double kStart = 1. * ps, kActivation = 1.001 * ps;

struct Tally
{
  int newOldAtActivation = 0, newOldLater = 0, oldOldAtStart = 0;
};
Tally tally;

bool Pair(const G4Track& a, const G4Track& b, int x, int y)
{
  return (a.GetTrackID() == x && b.GetTrackID() == y) || (a.GetTrackID() == y && b.GetTrackID() == x);
}

class ContactAction : public G4UserTimeStepAction
{
  public:
    void UserReactionAction(const G4Track& a, const G4Track& b, const std::vector<G4Track*>*) override
    {
      const double now = G4Scheduler::Instance()->GetGlobalTime();
      if (Pair(a, b, kOld, kNew)) {
        if (now == kActivation) ++tally.newOldAtActivation;
        else ++tally.newOldLater;
      }
      if (Pair(a, b, kOldA, kOldB) && now == kStart) ++tally.oldOldAtStart;
    }
};

void Add(const std::string& species, double time, const G4ThreeVector& pos, int id)
{
  auto* config = G4MolecularConfiguration::GetMolecularConfiguration(species);
  if (!config) throw std::runtime_error("missing configuration " + species);
  auto* track = (new G4Molecule(config))->BuildTrack(time, pos);
  track->SetTrackID(id);
  track->SetTrackStatus(fAlive);
#if HAS_SOURCE_ACTIVATION
  // As SourceInjector: every injected molecule is registered at its activation.
  G4DNAIndependentReactionTimeStepper::RegisterSourceActivation(id, time);
#endif
  G4ITTrackHolder::Instance()->Push(track);
}

// One history; prints "TRIAL <atActivation> <later> <oldOld> <P> <Reff_nm>".
int Trial(const std::string& newSpecies, long seed)
{
  std::unique_ptr<G4RunManager> manager(G4RunManagerFactory::CreateRunManager(G4RunManagerType::Serial));
  OutputFormatsMessenger formats;
  manager->SetUserInitialization(new DetectorConstruction);
  manager->SetUserInitialization(new PhysicsList);
  manager->SetUserInitialization(new ActionInitialization);
  for (const char* cmd : {"/run/verbose 0", "/process/chem/TimeStepModel IRT_syn", "/chem/meso/enable false",
                          "/chem/env/halfBox 250 um", "/output/defaultFormat none", "/run/initialize"})
    if (G4UImanager::GetUIpointer()->ApplyCommand(cmd)) throw std::runtime_error(std::string("macro: ") + cmd);
  G4Scheduler::Instance()->SetUserAction(new ContactAction);
  G4Scheduler::Instance()->SetEndTime(2 * ps);
  G4Scheduler::Instance()->SetVerbose(0);

  auto* electron = G4MolecularConfiguration::GetMolecularConfiguration("e_aq");
  auto* partner = G4MolecularConfiguration::GetMolecularConfiguration(newSpecies);
  if (!electron || !partner) throw std::runtime_error("unknown species " + newSpecies);
  auto* data = G4DNAMolecularReactionTable::GetReactionTable()->GetReactionData(electron, partner);
  if (!data) throw std::runtime_error("e_aq + " + newSpecies + " is not a reaction");
  const double reff = data->GetEffectiveReactionRadius();
  if (reff <= 0.01 * nm) throw std::runtime_error("effective radius below the 0.01 nm separation");

#if HAS_SOURCE_ACTIVATION
  G4DNAIndependentReactionTimeStepper::ResetSourceActivations();
#endif
  Add("e_aq", kStart, {0, 0, 0}, kOld);
  Add(newSpecies, kActivation, {0.01 * nm, 0, 0}, kNew);
  Add("e_aq", kStart, {1 * um, 0, 0}, kOldA);
  Add("e_aq", kStart, {1 * um + 0.01 * nm, 0, 0}, kOldB);

  G4Random::setTheSeed(seed);
  G4DNAChemistryManager::Instance()->UseAsStandalone(true);
  G4DNAChemistryManager::Instance()->Run();
  std::printf("TRIAL %d %d %d %.12g %.12g\n", tally.newOldAtActivation, tally.newOldLater, tally.oldOldAtStart,
              data->GetProbability(), reff / nm);
  std::fflush(stdout);
  return 0;
}
}  // namespace

int main(int argc, char** argv)
{
  const std::string species = PatchCheck::Option(argc, argv, "species", "e_aq");
  try {
    const std::string seed = PatchCheck::Option(argc, argv, "trial", "");
    if (!seed.empty()) return Trial(species, std::stol(seed));
  }
  catch (const std::exception& e) {
    std::fprintf(stderr, "TRIAL_FAIL %s\n", e.what());
    return 2;
  }

  const auto expect = PatchCheck::ParseExpect(argc, argv);
  PatchCheck::Checker c("source-contact", expect);
  const int n = std::stoi(PatchCheck::Option(argc, argv, "trials", "20"));
  c.Result("hasSourceActivation", HAS_SOURCE_ACTIVATION);
  c.Result("pair", "e_aq+" + species);

  int atActivation = 0, later = 0, oldOld = 0, failed = 0;
  double p = -1, reff = -1;
  for (int i = 0; i < n; ++i) {
    const std::string cmd = "\"\"" + std::string(argv[0]) + "\" --trial " + std::to_string(1729 + 4 * i)
                            + " --species " + species + " 2>&1\"";
    FILE* pipe = _popen(cmd.c_str(), "r");
    if (!pipe) { ++failed; continue; }
    char line[1024];
    bool got = false;
    while (std::fgets(line, sizeof line, pipe)) {
      int a = 0, l = 0, o = 0;
      double tp = 0, tr = 0;
      if (std::sscanf(line, "TRIAL %d %d %d %lf %lf", &a, &l, &o, &tp, &tr) == 5) {
        atActivation += a; later += l; oldOld += o; p = tp; reff = tr; got = true;
      }
      else if (std::string(line).rfind("MODULE", 0) == 0 || std::string(line).rfind("TRIAL_FAIL", 0) == 0)
        std::fputs(line, stdout);
    }
    if (_pclose(pipe) != 0 || !got) ++failed;
  }
  // The kernel under test, from this (parent) process: same PATH as the trials.
  c.Module("G4processes_core", "G4processes_core.dll");

  const double mean = n * p, sigma = std::sqrt(n * p * (1. - p));
  c.Result("trials", n);
  c.Result("failedTrials", failed);
  c.Result("probability", p);
  c.Result("reff_nm", reff);
  c.Result("newOldAtActivation", atActivation);
  c.Result("newOldLater", later);
  c.Result("expected", mean);
  c.Result("sigma", sigma);
  c.Result("oldOldAtStart", oldOld);
  c.Check(failed == 0, "every trial ran");
  c.Check(oldOld == n, "the old-old start-time contact reacts in every trial (control)");
  if (expect == PatchCheck::Expect::Fixed) {
    c.Check(HAS_SOURCE_ACTIVATION == 1, "the kernel exposes RegisterSourceActivation");
    c.Check(atActivation > 0, "new-old contacts react at activation");
    c.Check(std::abs(atActivation - mean) <= 4. * sigma + 1e-9, "count within 4 sigma of N*P");
  }
  else {
    c.Check(atActivation == 0, "no new-old contact reacts at activation");
  }
  return c.Finish();
}
