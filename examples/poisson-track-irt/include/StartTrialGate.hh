// StartTrialGate: switches off the IRT_syn start-time contact trial for one
// scope, in the stock build (ticket 07).
//
// Why. In `history` the first packet is in the history at the scheduler start
// time. G4DNAIndependentReactionTimeStepper::CalculateStep (Geant4 11.4.3,
// G4DNAIndependentReactionTimeStepper.cc:167-173) gives every pair within the
// effective reaction radius a contact trial, GetProbability() > G4UniformRand(),
// but only while GetGlobalTime() == GetStartTime(); success means a reaction at
// time step 0. `capture` has already resolved those contacts (the stock build
// captures after the start-up contacts), so the first packet would get them a
// second time, and no later packet would. Measured over 64 packets: 4.80 +- 0.27
// extra zero-time reactions per first packet, with sum p = 3.24
// (.scratch/tests/2026-11-10__poisson-track-irt-stock-build/mitigation-spike/README.md).
//
// How. The constructor saves the probability of every reaction data of the
// table and sets it to 0, so the trial never succeeds (it still draws its
// G4UniformRand, so the random sequence matches the ungated stepper only up to
// the first trial whose outcome differs; pairing with an ungated run is
// partial). The destructor restores the saved values exactly, also when the
// scope is left by an exception. The first packet is then treated like every
// later packet: a within-radius pair whose trial fails goes on to
// CheckAndRecordResults with the stepper's current minimum time step (stepper
// .cc:166-189), as every later packet's pairs do; it gets no sampled time of its own.
//
// Safety, from the 11.4.3 source:
//   - GetProbability() has two readers in source/processes/electromagnetic/dna:
//     the start-time trial above (IRT_syn) and G4DNAIRT.cc:332 (classic IRT,
//     which `history` refuses). CMakeLists.txt therefore requires Geant4 11.4.x
//     in the stock build; another version needs this audit again.
//   - The table hands out const data (G4DNAMolecularReactionTable::Data), but it
//     was created non-const with `new` (G4EmDNAChemistry_option3.cc) and is only
//     owned through unique_ptr<const Data>, so the const_cast write is defined
//     (Geant4 does the same in ScaleReactionRateForNewTemperature,
//     G4DNAMolecularReactionTable.cc:811).
//   - Nothing writes the probability during a run: ComputeEffectiveRadius and
//     SetReactionType write it when the table is built, SetProbability has no
//     caller in the DNA source.
//
// Use it in the stock build only, around G4DNAChemistryManager::Run(); the
// patched build trials fresh sources itself and keeps the kernel's behaviour.
#pragma once

#include <cstddef>
#include <utility>
#include <vector>

#include "globals.hh"

class G4DNAMolecularReactionData;
class G4DNAMolecularReactionTable;

namespace ptirt
{

class StartTrialGate
{
 public:
  // Gates every reaction data of the table (GetVectorOfReactionData()).
  explicit StartTrialGate(G4DNAMolecularReactionTable& table);
  // Gates the given data; null entries are skipped, a repeated entry is restored
  // to its value before the gate.
  explicit StartTrialGate(const std::vector<const G4DNAMolecularReactionData*>& data);
  ~StartTrialGate();

  StartTrialGate(const StartTrialGate&) = delete;
  StartTrialGate& operator=(const StartTrialGate&) = delete;
  StartTrialGate(StartTrialGate&&) = delete;
  StartTrialGate& operator=(StartTrialGate&&) = delete;

  // The number of reaction data whose probability is held at 0.
  std::size_t Gated() const { return fSaved.size(); }

 private:
  std::vector<std::pair<G4DNAMolecularReactionData*, G4double>> fSaved;
};

}  // namespace ptirt
