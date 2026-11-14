#include "SourceInjector.hh"

#include <array>
#include <cmath>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>

#include "G4DNAIndependentReactionTimeStepper.hh"
#include "G4ElectronOccupancy.hh"
#include "G4ITTrackHolder.hh"
#include "G4MolecularConfiguration.hh"
#include "G4Molecule.hh"
#include "G4MoleculeCounterManager.hh"
#include "G4MoleculeDefinition.hh"
#include "G4Scheduler.hh"
#include "G4SystemOfUnits.hh"
#include "G4Track.hh"

namespace ptirt
{

namespace
{

std::string Where(const Arrival& arrival)
{
  return "source " + std::to_string(arrival.sourceIndex) + " (packet " +
         std::to_string(arrival.packetIndex) + ")";
}

std::vector<int> Occupancy(const G4MolecularConfiguration& configuration)
{
  std::vector<int> occupancy;
  if (const auto* electrons = configuration.GetElectronOccupancy())
    for (G4int i = 0; i < electrons->GetSizeOfOrbit(); ++i) occupancy.push_back(electrons->GetOccupancy(i));
  return occupancy;
}

// The configuration a packet molecule was captured in, matched on the same
// fields CaptureRecorder wrote: definition name, charge, decay time, electron
// occupancy. Exactly one must match.
const G4MolecularConfiguration* Resolve(const PacketMolecule& molecule, const std::string& where)
{
  const G4MolecularConfiguration* found = nullptr;
  std::size_t matches = 0;
  for (G4int i = 0; i < G4MolecularConfiguration::GetNumberOfSpecies(); ++i)
  {
    const auto* configuration = G4MolecularConfiguration::GetMolecularConfiguration(i);
    if (configuration == nullptr || configuration->GetDefinition() == nullptr) continue;
    if (configuration->GetDefinition()->GetParticleName() != molecule.definitionName) continue;
    if (configuration->GetCharge() != molecule.charge) continue;
    if (configuration->GetDecayTime() / ns != molecule.decayTimeNs) continue;
    if (Occupancy(*configuration) != molecule.electronOccupancy) continue;
    found = configuration;
    ++matches;
  }
  if (matches != 1)
  {
    std::ostringstream text;
    text << "incompatible packet: " << where << ": molecule " << molecule.definitionName << " (charge "
         << molecule.charge << ", " << molecule.electronOccupancy.size() << " occupancy levels) matches "
         << matches << " molecular configurations of this build, expected exactly 1";
    throw std::runtime_error(text.str());
  }
  return found;
}

G4bool StrictlyInside(const std::array<double, 3>& positionNm, double halfSideNm)
{
  for (double x : positionNm)
    if (!(std::abs(x) < halfSideNm)) return false;
  return true;
}

// Builds the tracks with the molecule counter inactive (see the header) and
// restores its state afterwards, also on an exception.
class CounterStagingGuard
{
 public:
  CounterStagingGuard() : fManager(G4MoleculeCounterManager::Instance())
  {
    if (!fManager->GetIsActive())
      throw std::runtime_error("the molecule counter manager is inactive; the history could not be counted");
    fManager->SetIsActive(false);
  }
  ~CounterStagingGuard() { fManager->SetIsActive(true); }
  CounterStagingGuard(const CounterStagingGuard&) = delete;
  CounterStagingGuard& operator=(const CounterStagingGuard&) = delete;

 private:
  G4MoleculeCounterManager* fManager;
};

}  // namespace

SourceInjector::SourceInjector(const std::vector<Arrival>& arrivals, const PacketLibrary& library,
                               G4double worldHalfSide, G4double boxHalfSide, G4double endpoint)
{
  const double worldNm = worldHalfSide / nm;
  const double boxNm = boxHalfSide / nm;
  const double endpointNs = endpoint / ns;
  if (!(worldNm > 0) || !(boxNm >= 0) || !(endpointNs > 0) || !std::isfinite(endpointNs))
    throw std::runtime_error("invalid world, box or endpoint");

  std::size_t total = 0;
  for (const auto& arrival : arrivals)
  {
    if (arrival.packetIndex >= library.packets.size())
      throw std::runtime_error(Where(arrival) + ": the library has only " +
                               std::to_string(library.packets.size()) + " packets");
    total += library.packets[arrival.packetIndex].molecules.size();
  }
  if (total > static_cast<std::size_t>(std::numeric_limits<G4int>::max()))
    throw std::runtime_error("too many molecules for G4int track IDs");
  fMolecules.reserve(total);

  for (const auto& arrival : arrivals)
  {
    const std::string where = Where(arrival);
    const auto& packet = library.packets[arrival.packetIndex];
    const auto& entry = library.entries[arrival.packetIndex];
    if (packet.molecules.empty()) throw std::runtime_error(where + ": the packet holds no molecule");

    // The packet's molecules all sit at its capture age; the source activates
    // that long after its arrival, and must do so before the endpoint.
    const double activationNs = arrival.timeNs + packet.captureTimeNs;
    if (!std::isfinite(activationNs) || !(activationNs >= 0) || !(activationNs < endpointNs))
    {
      std::ostringstream text;
      text.precision(17);
      text << where << " activates at " << activationNs << " ns (arrival " << arrival.timeNs
           << " ns + capture age " << packet.captureTimeNs << " ns), not before the endpoint " << endpointNs
           << " ns";
      throw std::runtime_error(text.str());
    }

    const auto translated = Translate(packet, arrival.offsetNm);  // rigid: positions only
    InjectedSource source;
    source.sourceIndex = arrival.sourceIndex;
    source.activationTime = activationNs * ns;
    source.firstTrackId = static_cast<G4int>(fMolecules.size()) + 1;
    for (std::size_t i = 0; i < translated.molecules.size(); ++i)
    {
      const auto& captured = translated.molecules[i];
      const std::string what = where + " molecule " + std::to_string(i);
      if (captured.globalTimeNs != packet.captureTimeNs)
        throw std::runtime_error(what + ": its global time differs from the packet's capture age");
      if (!StrictlyInside(captured.positionNm, worldNm))
        throw std::runtime_error(what + ": lies outside the world after translation");
      if (boxNm > 0 && !StrictlyInside(captured.positionNm, boxNm))
        throw std::runtime_error(what + ": lies outside the reflecting box after translation");

      Molecule molecule;
      molecule.configuration = Resolve(captured, where);
      const auto& p = captured.positionNm;
      const auto& d = captured.direction;
      molecule.position = G4ThreeVector(p[0], p[1], p[2]) * nm;
      molecule.direction = G4ThreeVector(d[0], d[1], d[2]);
      molecule.status = static_cast<G4TrackStatus>(captured.status);  // fAlive 0, fStopButAlive 1
      molecule.kineticEnergy = captured.kineticEnergyEv * eV;
      molecule.localTime = captured.localTimeNs * ns;
      molecule.properTime = captured.properTimeNs * ns;
      molecule.source = fSources.size();
      fMolecules.push_back(molecule);
    }
    source.lastTrackId = static_cast<G4int>(fMolecules.size());
    fSources.push_back(source);

    ActivationRecord record;
    record.sourceIndex = arrival.sourceIndex;
    record.packetIndex = arrival.packetIndex;
    record.offsetNm = arrival.offsetNm;
    record.scheduledTimeNs = arrival.timeNs;
    record.expectedActivationTimeNs = activationNs;
    record.observedActivationTimeNs = std::numeric_limits<double>::quiet_NaN();
    record.firstTrackId = source.firstTrackId;
    record.lastTrackId = source.lastTrackId;
    record.moleculeCount = translated.molecules.size();
    record.energyDepositEv = entry.energyDepositEv;
    record.escapedEnergyEv = entry.escapedEnergyEv;
    fRecords.push_back(record);
  }
}

void SourceInjector::Inject()
{
  if (fInjected) throw std::runtime_error("the sources were already injected");
  if (G4Scheduler::Instance()->IsRunning())
    throw std::runtime_error("cannot inject while the chemistry is running");
  fInjected = true;

  // One registry per history: forget the activations of any earlier history.
  G4DNAIndependentReactionTimeStepper::ResetSourceActivations();

  CounterStagingGuard guard;
  auto* holder = G4ITTrackHolder::Instance();
  for (std::size_t i = 0; i < fMolecules.size(); ++i)
  {
    const auto& staged = fMolecules[i];
    const G4double activation = fSources[staged.source].activationTime;
    const auto trackId = static_cast<G4int>(i) + 1;
    G4Track* track = (new G4Molecule(staged.configuration))->BuildTrack(activation, staged.position);
    track->SetTrackID(trackId);
    track->SetTrackStatus(staged.status);
    track->SetMomentumDirection(staged.direction);
    track->SetKineticEnergy(staged.kineticEnergy);
    track->SetLocalTime(staged.localTime);
    track->SetProperTime(staged.properTime);
    G4DNAIndependentReactionTimeStepper::RegisterSourceActivation(trackId, activation);
    // Not running yet: the holder files the track in the delayed list of its
    // global time, i.e. its activation time.
    holder->Push(track);
  }
}

std::vector<ActivationRecord> SourceInjector::MakeActivationRecords() const
{
  return fRecords;
}

}  // namespace ptirt
