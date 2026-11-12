#include "CaptureRecorder.hh"

#include <algorithm>
#include <cmath>
#include <exception>
#include <fstream>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <utility>

#include "BuildMode.hh"
#include "CoincidenceCheck.hh"
#include "ExamplePhysicsList.hh"
#include "ExamplePhysicsTag.hh"
#include "PacketDigest.hh"

#include "G4ElectronOccupancy.hh"
#include "G4Event.hh"
#include "G4ITTrackHolder.hh"
#include "G4MolecularConfiguration.hh"
#include "G4Molecule.hh"
#include "G4MoleculeDefinition.hh"
#include "G4ParticleDefinition.hh"
#include "G4ParticleGun.hh"
#include "G4RunManager.hh"
#include "G4Scheduler.hh"
#include "G4SystemOfUnits.hh"
#include "G4Track.hh"
#include "G4TrackList.hh"
#include "Randomize.hh"

// The precursor age named by the compatibility tag is Geant4's own 1 ps.
static_assert(picosecond / ns == ptirt::kPrecursorAgeNs, "precursor age must be 1 ps");

namespace ptirt
{

namespace
{

std::array<double, 3> ToNm(const G4ThreeVector& v)
{
  return {v.x() / nm, v.y() / nm, v.z() / nm};
}

std::array<double, 3> ToArray(const G4ThreeVector& v)
{
  return {v.x(), v.y(), v.z()};
}

// Copies the state of a chemistry track into `state`; returns a problem
// description, empty when the track is a molecule that can be captured.
std::string DescribeTrack(const G4Track& track, const G4ThreeVector& origin, PacketMolecule& state)
{
  const auto* molecule = GetMolecule(track);
  if (molecule == nullptr) return "a chemistry track carries no G4Molecule";
  const auto* configuration = molecule->GetMolecularConfiguration();
  state = PacketMolecule{};
  state.definitionName = configuration->GetDefinition()->GetParticleName();
  state.charge = configuration->GetCharge();
  state.status = static_cast<int>(track.GetTrackStatus());  // fAlive 0, fStopButAlive 1
  if (const auto* occupancy = configuration->GetElectronOccupancy())
    for (G4int i = 0; i < occupancy->GetSizeOfOrbit(); ++i)
      state.electronOccupancy.push_back(occupancy->GetOccupancy(i));
  state.positionNm = ToNm(track.GetPosition() - origin);
  state.direction = ToArray(track.GetMomentumDirection());
  state.globalTimeNs = track.GetGlobalTime() / ns;
  state.localTimeNs = track.GetLocalTime() / ns;
  state.properTimeNs = track.GetProperTime() / ns;
  state.decayTimeNs = configuration->GetDecayTime() / ns;
  state.kineticEnergyEv = track.GetKineticEnergy() / eV;
  return {};
}

}  // namespace

CaptureRecorder::CaptureRecorder(CaptureSettings settings) : fSettings(std::move(settings)) {}

void CaptureRecorder::Fail(const std::string& message)
{
  if (fFailure.empty())
  {
    std::ostringstream text;
    text << "event " << fEventId << ": " << message;
    fFailure = text.str();
  }
  // Inside G4Scheduler::Process this ends the chemistry before its next step;
  // outside it, Process() resets the flag, so the call is harmless.
  G4Scheduler::Instance()->Stop();
  if (auto* runManager = G4RunManager::GetRunManager()) runManager->AbortRun(/*softAbort*/ true);
}

std::string CaptureRecorder::DescribeGunProblem(G4ParticleGun& gun) const
{
  if (!(fHalfSide > 0)) return "the capture half side was not set before the first event";
  if (gun.GetNumberOfParticles() != 1)
    return "the gun must shoot exactly one particle per event (/gun/number 1)";
  if (gun.GetParticleDefinition() == nullptr) return "the gun has no particle (/gun/particle)";
  if (!(gun.GetParticleEnergy() > 0)) return "the gun energy must be positive (/gun/energy)";
  // Geant4-DNA stamps every precursor at 1 ps whatever the track time, so a
  // gun time offset would be silently dropped; activation times belong to the
  // injection schedule instead.
  if (gun.GetParticleTime() != 0)
    return "the gun time must be 0 (/gun/time); set activation times in the schedule";
  const auto direction = gun.GetParticleMomentumDirection();
  if (std::abs(direction.mag() - 1) > fSettings.limits.directionNormTol)
    return "the gun direction must be a unit vector (/gun/direction)";
  const auto position = gun.GetParticlePosition();
  if (!(std::abs(position.x()) < fHalfSide && std::abs(position.y()) < fHalfSide &&
        std::abs(position.z()) < fHalfSide))
    return "the gun position must be strictly inside the capture box (/gun/position, "
           "/ptirt/capture/halfSide)";
  return {};
}

void CaptureRecorder::BeginEvent(G4int eventId, G4ParticleGun& gun)
{
  fEventId = eventId;
  fPhysicsSeed = fSettings.firstPhysicsSeed + eventId;
  fDeposit = 0;
  fEscaped = 0;
  fChemistryStarted = false;
  fCaptured = false;
  fMolecules.clear();
  fMoleculeSpecies.clear();
  fChemistryStartDigest = 0;
  fCapturedDigest = 0;
  fChemistrySeed = 0;
  fStartupZeroSteps = 0;
  fStartupReactions = 0;
  fProductReaction.clear();

  // Declared seed: everything random in this event (physics and the molecule
  // directions drawn by G4Molecule::BuildTrack) follows from it.
  G4Random::setTheSeed(static_cast<long>(fPhysicsSeed));

  if (const auto problem = DescribeGunProblem(gun); !problem.empty())
  {
    Fail(problem);
    return;
  }
  fPrimaryParticle = gun.GetParticleDefinition()->GetParticleName();
  fPrimaryEnergy = gun.GetParticleEnergy();
  fOrigin = gun.GetParticlePosition();
  fDirection = gun.GetParticleMomentumDirection();
}

void CaptureRecorder::AddDeposit(G4double energyDeposit)
{
  fDeposit += energyDeposit;
}

void CaptureRecorder::EndTrack(const G4Track& track)
{
  // Only a track that leaves the world takes energy out of the box. A track
  // killed inside it (e.g. by G4DNAOneStepThermalizationModel, which deposits
  // the whole kinetic energy and leaves a nominal 25 meV on the dead track)
  // has already deposited everything.
  if (track.GetNextVolume() == nullptr) fEscaped += track.GetKineticEnergy();
}

void CaptureRecorder::StartChemistry()
{
  fChemistryStarted = true;
  if (!fFailure.empty())
  {
    G4Scheduler::Instance()->Stop();
    return;
  }

  // G4DNAChemistryManager builds every precursor at exactly `picosecond`.
  const G4double captureTime = picosecond;
  if (const auto problem = DescribeIrtSynSchedulerProblem(captureTime); !problem.empty())
  {
    Fail(problem);
    return;
  }

  std::vector<PacketMolecule> waiting;
  for (const auto& [time, lists] : G4ITTrackHolder::Instance()->GetDelayedLists())
  {
    if (time != captureTime)
    {
      std::ostringstream text;
      text << "a delayed list holds molecules at " << time / ns
           << " ns, not at the 1 ps precursor age (pulse or delayed-time information?)";
      Fail(text.str());
      return;
    }
    for (const auto& [key, list] : lists)
    {
      (void)key;
      if (list == nullptr) continue;
      for (auto* track : *list)
      {
        PacketMolecule state;
        if (const auto problem = DescribeTrack(*track, fOrigin, state); !problem.empty())
        {
          Fail(problem);
          return;
        }
        if (track->GetGlobalTime() != captureTime)
        {
          Fail("a molecule's global time differs from its delayed-list time");
          return;
        }
        waiting.push_back(std::move(state));
      }
    }
  }
  fChemistryStartDigest = DigestMolecules(waiting);

  if constexpr (kPatchedGeant4)
  {
    // Keep the delayed-list molecules but run no chemistry step
    // (G4Scheduler::Process then ends the tracks and clears the lists).
    fMolecules = std::move(waiting);
    fCapturedDigest = fChemistryStartDigest;
    fCaptured = true;
    G4Scheduler::Instance()->Stop();
  }
  else
  {
    // The stock kernel gives no hook before the start-up contacts, so the
    // chemistry runs on; its random sequence is restarted here so that it
    // depends on the declared physics seed alone (CaptureMain refuses the
    // physics seeds for which that cannot be done).
    if (fPhysicsSeed < 1 || static_cast<std::uint64_t>(fPhysicsSeed) > kMaxStockPhysicsSeed)
    {
      Fail("physics seed " + std::to_string(fPhysicsSeed) + " is outside 1.." +
           std::to_string(kMaxStockPhysicsSeed) + " in the stock build");
      return;
    }
    fChemistrySeed = ChemistrySeedFor(static_cast<std::uint64_t>(fPhysicsSeed));
    G4Random::setTheSeed(static_cast<long>(fChemistrySeed));
  }
}

void CaptureRecorder::BeforeTimeStep(G4double timeStep)
{
  // The patched build captured at StartChemistry and stopped the chemistry.
  if constexpr (!kPatchedGeant4)
  {
    if (!fChemistryStarted || fCaptured || !fFailure.empty()) return;

    if (timeStep < 0)
    {
      Fail("PTIRT_CAPTURE_STATE: the chemistry chose a negative time step");
      return;
    }
    if (timeStep == 0)
    {
      ++fStartupZeroSteps;
      return;
    }
    CaptureStartupState();
  }
}

void CaptureRecorder::OnReaction(const G4Track& trackA, const G4Track& trackB,
                                 const std::vector<G4Track*>* products)
{
  // The patched build runs no chemistry step before its capture.
  if constexpr (!kPatchedGeant4)
  {
    if (!fChemistryStarted || fCaptured || !fFailure.empty()) return;

    ++fStartupReactions;
    // Geant4 kills both reactants of a reaction that reaches this point.
    fProductReaction.erase(&trackA);
    fProductReaction.erase(&trackB);
    if (products == nullptr) return;
    for (const auto* product : *products)
    {
      if (product == nullptr)
      {
        Fail("PTIRT_CAPTURE_STATE: a start-up reaction lists a null product");
        return;
      }
      fProductReaction[product] = fStartupReactions;
    }
  }
}

// Stock build, at the first chemistry step of positive length: every molecule is
// alive in the main list, at the precursor age, with nothing waiting anywhere.
void CaptureRecorder::CaptureStartupState()
{
  const G4double captureTime = picosecond;
  auto* scheduler = G4Scheduler::Instance();
  auto* holder = G4ITTrackHolder::Instance();

  if (scheduler->GetGlobalTime() != captureTime)
  {
    std::ostringstream text;
    text << "PTIRT_CAPTURE_STATE: the first step of positive length starts at "
         << scheduler->GetGlobalTime() / ns << " ns, not at 1 ps";
    Fail(text.str());
    return;
  }
  if (holder->DelayListsNOTEmpty())
  {
    Fail("PTIRT_CAPTURE_STATE: molecules are still waiting in a delayed list");
    return;
  }

  struct Captured
  {
    PacketMolecule molecule;
    const G4Track* track = nullptr;
    const G4MolecularConfiguration* configuration = nullptr;
    int trackId = 0;  // tie-break only
  };
  std::vector<Captured> captured;
  for (const auto& [key, lists] : holder->GetLists())
  {
    (void)key;
    if (lists == nullptr) continue;
    for (const auto type : {PriorityList::SecondariesList, PriorityList::WaitingList})
    {
      const auto* list = lists->Get(type);
      if (list != nullptr && !list->empty())
      {
        Fail(std::string("PTIRT_CAPTURE_STATE: molecules are still in a ") +
             (type == PriorityList::SecondariesList ? "secondaries" : "waiting") + " list");
        return;
      }
    }
    G4TrackList* mainList = lists->GetMainList();
    if (mainList == nullptr) continue;
    for (auto* track : *mainList)
    {
      if (track->GetTrackStatus() != fAlive)
      {
        std::ostringstream text;
        text << "PTIRT_CAPTURE_STATE: track " << track->GetTrackID() << " has status "
             << static_cast<int>(track->GetTrackStatus()) << " (fAlive expected)";
        Fail(text.str());
        return;
      }
      if (track->GetGlobalTime() != captureTime)
      {
        std::ostringstream text;
        text << "PTIRT_CAPTURE_STATE: track " << track->GetTrackID() << " is at "
             << track->GetGlobalTime() / ns << " ns, not at 1 ps";
        Fail(text.str());
        return;
      }
      Captured entry;
      entry.track = track;
      entry.configuration = GetMolecule(*track)->GetMolecularConfiguration();
      entry.trackId = track->GetTrackID();
      if (const auto problem = DescribeTrack(*track, fOrigin, entry.molecule); !problem.empty())
      {
        Fail(problem);
        return;
      }
      captured.push_back(std::move(entry));
    }
  }

  // Geant4 lists the molecules in an order that follows memory addresses. The
  // packet lists them in canonical order instead, so that it depends on the
  // physics seed alone.
  std::sort(captured.begin(), captured.end(), [](const Captured& a, const Captured& b) {
    if (CanonicalLess(a.molecule, b.molecule)) return true;
    if (CanonicalLess(b.molecule, a.molecule)) return false;
    return a.trackId < b.trackId;
  });

  // Product groups: dense ids 1..G by first appearance in that order; the
  // molecules of one reaction share a group.
  std::map<int, int> groupOfReaction;
  fMolecules.clear();
  fMolecules.reserve(captured.size());
  fMoleculeSpecies.clear();
  fMoleculeSpecies.reserve(captured.size());
  for (auto& entry : captured)
  {
    fMoleculeSpecies.push_back(fRadii.Label(entry.configuration));
    if (const auto it = fProductReaction.find(entry.track); it != fProductReaction.end())
      entry.molecule.productGroup =
          groupOfReaction.try_emplace(it->second, static_cast<int>(groupOfReaction.size()) + 1)
              .first->second;
    fMolecules.push_back(std::move(entry.molecule));
  }
  fCapturedDigest = DigestMolecules(fMolecules);
  fCaptured = true;

  // The step that is about to start still runs to its end (a reaction or two may
  // fire); the copy above is not affected.
  scheduler->Stop();
}

// Within-Reff pairs of the captured molecules (all alive) and the sum of the
// contact probabilities that G4DNAIndependentReactionTimeStepper applies to them
// at the start time: reactionData->GetProbability() > G4UniformRand() per pair.
SecondTrial CaptureRecorder::ComputeSecondTrial(std::map<std::string, std::pair<std::size_t, double>>& perPair)
{
  std::vector<ContactMolecule> molecules(fMolecules.size());
  for (std::size_t i = 0; i < fMolecules.size(); ++i)
  {
    molecules[i].species = fMoleculeSpecies[i];
    molecules[i].positionNm = fMolecules[i].positionNm;
  }
  const auto counts = CountInternalContacts(molecules, fRadii.AsFunction());

  std::map<std::string, double> probabilityOf;  // PairLabel -> GetProbability()
  std::set<std::string> species(fMoleculeSpecies.begin(), fMoleculeSpecies.end());
  for (auto a = species.begin(); a != species.end(); ++a)
    for (auto b = a; b != species.end(); ++b) probabilityOf[PairLabel(*a, *b)] = fRadii.Probability(*a, *b);

  SecondTrial trial;
  trial.pairs = counts.pairs;
  for (const auto& [label, n] : counts.perPair)
  {
    const auto it = probabilityOf.find(label);
    if (it == probabilityOf.end()) throw std::runtime_error("no probability for pair '" + label + "'");
    perPair[label] = {n, static_cast<double>(n) * it->second};
    trial.expectedReactions += perPair[label].second;
  }
  return trial;
}

void CaptureRecorder::EndEvent(const G4Event& event)
{
  if (!fFailure.empty()) return;
  if (event.IsAborted())
  {
    Fail("the event was aborted; its energy is not fully accounted for");
    return;
  }
  if (!fChemistryStarted)
  {
    Fail("the chemistry never started (G4DNAChemistryManager::Run was not reached)");
    return;
  }
  if (!fCaptured)
  {
    Fail("PTIRT_CAPTURE_STATE: the chemistry ended before its first step of positive length (" +
         std::to_string(fStartupZeroSteps) + " zero-length steps, " +
         std::to_string(fStartupReactions) + " start-up reactions)");
    return;
  }
  if (fMolecules.empty())
  {
    Fail("the primary produced no precursor molecule in the capture box");
    return;
  }

  // Two molecules at one point cannot be injected (CoincidenceCheck.hh); the
  // products of one start-up reaction are the only allowed exception.
  std::vector<CoincidenceMolecule> placed(fMolecules.size());
  for (std::size_t i = 0; i < fMolecules.size(); ++i)
  {
    placed[i].position = fMolecules[i].positionNm;
    placed[i].productGroup = fMolecules[i].productGroup;
  }
  CoincidenceSummary coincidence;
  try
  {
    const auto coincidences = FindCoincidentPositions(placed);
    if (coincidences.forbiddenMolecules > 0)
    {
      Fail(DescribeForbiddenCoincidences(coincidences));
      return;
    }
    // The patched capture has no product groups, hence nothing exempt and no species labels.
    coincidence = SummarizeExempt(coincidences, fMoleculeSpecies, fRadii.AsFunction());
  }
  catch (const std::exception& error)
  {
    Fail(std::string("coincidence check: ") + error.what());
    return;
  }

  // Stock only: what the IRT_syn stepper's first trial would see in this packet.
  std::optional<SecondTrial> secondTrial;
  std::map<std::string, std::pair<std::size_t, double>> secondTrialPerPair;
  if constexpr (!kPatchedGeant4)
  {
    try
    {
      secondTrial = ComputeSecondTrial(secondTrialPerPair);
    }
    catch (const std::exception& error)
    {
      Fail(std::string("second trial: ") + error.what());
      return;
    }
  }

  PrecursorPacket packet;
  packet.compatibilityTag = kPacketCompatibilityTag;
  packet.primaryParticle = fPrimaryParticle;
  packet.primaryEnergyEv = fPrimaryEnergy / eV;
  packet.energyDepositEv = fDeposit / eV;
  packet.escapedEnergyEv = fEscaped / eV;
  packet.captureHalfSideNm = fHalfSide / nm;
  packet.captureTimeNs = picosecond / ns;
  packet.molecules = std::move(fMolecules);
  fMolecules.clear();

  const auto index = static_cast<std::size_t>(fEventId);
  const auto path = fSettings.outputDirectory / PacketFileName(index);
  try
  {
    {
      std::ofstream out(path, std::ios::binary);
      if (!out) throw std::runtime_error("cannot open " + path.string());
      Write(out, packet, fSettings.limits);  // validates, including the energy balance
      out.close();
      if (!out) throw std::runtime_error("cannot finish writing " + path.string());
    }
    std::ifstream in(path, std::ios::binary);
    const auto reread = Read(in, kPacketCompatibilityTag, fSettings.limits);
    if (!(reread == packet))
      throw std::runtime_error(path.string() + " does not read back to the captured packet");
    fEntries.push_back(MakeLibraryEntry(index, fPhysicsSeed, packet));
    fEntries.back().secondTrial = secondTrial;
  }
  catch (const std::exception& error)
  {
    Fail(std::string("packet ") + path.filename().string() + ": " + error.what());
    return;
  }

  CaptureProvenance provenance;
  provenance.primaryOriginNm = ToNm(fOrigin);
  provenance.primaryDirection = ToArray(fDirection);
  provenance.chemistryStartDigest = fChemistryStartDigest;
  provenance.capturedDigest = fCapturedDigest;
  provenance.chemistrySeed = fChemistrySeed;
  provenance.startupZeroSteps = fStartupZeroSteps;
  provenance.startupReactions = fStartupReactions;
  provenance.coincidence = coincidence;
  provenance.secondTrialPerPair = std::move(secondTrialPerPair);
  fProvenance.push_back(provenance);
}

}  // namespace ptirt
