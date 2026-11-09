#include "CaptureRecorder.hh"

#include <cmath>
#include <exception>
#include <fstream>
#include <sstream>
#include <utility>

#include "ExamplePhysicsList.hh"
#include "ExamplePhysicsTag.hh"

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
  // Inside G4Scheduler::Process this ends the chemistry before its first step;
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
  fMolecules.clear();

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

void CaptureRecorder::CaptureAtChemistryStart()
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
        auto* molecule = GetMolecule(track);
        if (molecule == nullptr)
        {
          Fail("a chemistry track carries no G4Molecule");
          return;
        }
        if (track->GetGlobalTime() != captureTime)
        {
          Fail("a molecule's global time differs from its delayed-list time");
          return;
        }
        const auto* configuration = molecule->GetMolecularConfiguration();
        PacketMolecule state;
        state.definitionName = configuration->GetDefinition()->GetParticleName();
        state.charge = configuration->GetCharge();
        state.status = static_cast<int>(track->GetTrackStatus());  // fAlive 0, fStopButAlive 1
        if (const auto* occupancy = configuration->GetElectronOccupancy())
          for (G4int i = 0; i < occupancy->GetSizeOfOrbit(); ++i)
            state.electronOccupancy.push_back(occupancy->GetOccupancy(i));
        state.positionNm = ToNm(track->GetPosition() - fOrigin);
        state.direction = ToArray(track->GetMomentumDirection());
        state.globalTimeNs = track->GetGlobalTime() / ns;
        state.localTimeNs = track->GetLocalTime() / ns;
        state.properTimeNs = track->GetProperTime() / ns;
        state.decayTimeNs = configuration->GetDecayTime() / ns;
        state.kineticEnergyEv = track->GetKineticEnergy() / eV;
        fMolecules.push_back(std::move(state));
      }
    }
  }

  // Merge the delayed lists but run no chemistry step (G4Scheduler::Process
  // then ends the tracks and clears the lists).
  G4Scheduler::Instance()->Stop();
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
  if (fMolecules.empty())
  {
    Fail("the primary produced no precursor molecule in the capture box");
    return;
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
  }
  catch (const std::exception& error)
  {
    Fail(std::string("packet ") + path.filename().string() + ": " + error.what());
    return;
  }
  fProvenance.push_back({ToNm(fOrigin), ToArray(fDirection)});
}

}  // namespace ptirt
