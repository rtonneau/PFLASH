#include "RecordActions.hh"

#include <algorithm>
#include <cmath>
#include <sstream>

#include "RecordDetectorConstruction.hh"

#include "G4Event.hh"
#include "G4Exception.hh"
#include "G4GeneralParticleSource.hh"
#include "G4LogicalVolume.hh"
#include "G4RunManager.hh"
#include "G4Step.hh"
#include "G4SystemOfUnits.hh"
#include "G4Track.hh"
#include "G4VPhysicalVolume.hh"
#include "G4VTouchable.hh"

namespace phsp
{

namespace
{

// The touched copy's translation and the lattice centre of its copy number
// are composed differently (placement transforms vs ix P, iy P, zc); they
// agree to rounding.
constexpr G4double kCentreTolerance = 1e-9 * mm;
// A record lies on the box surface (Geant4's surface tolerance is 1e-9 mm).
constexpr G4double kSurfaceTolerance = 1e-8 * mm;

}  // namespace

// ---- RecordRun ----------------------------------------------------------------

void RecordRun::AddDepositWithoutRecord(G4double depositEv)
{
  ++fDepositWithoutRecordEvents;
  fDepositWithoutRecordEv += depositEv;
}

void RecordRun::Merge(const G4Run* run)
{
  const auto* other = static_cast<const RecordRun*>(run);
  fHistories.insert(fHistories.end(), std::make_move_iterator(other->fHistories.begin()),
                    std::make_move_iterator(other->fHistories.end()));
  other->fHistories.clear();
  fDepositWithoutRecordEvents += other->fDepositWithoutRecordEvents;
  fDepositWithoutRecordEv += other->fDepositWithoutRecordEv;
  G4Run::Merge(run);
}

// ---- Primary generator ----------------------------------------------------------

RecordPrimaryGeneratorAction::RecordPrimaryGeneratorAction() : fSource(std::make_unique<G4GeneralParticleSource>()) {}

RecordPrimaryGeneratorAction::~RecordPrimaryGeneratorAction() = default;

void RecordPrimaryGeneratorAction::GeneratePrimaries(G4Event* event)
{
  fSource->GeneratePrimaryVertex(event);
}

// ---- Stepping: inward crossings and in-box deposit --------------------------------

void RecordSteppingAction::UserSteppingAction(const G4Step* step)
{
  const G4StepPoint* pre = step->GetPreStepPoint();
  const G4VPhysicalVolume* volume = pre->GetPhysicalVolume();
  if (volume == nullptr || volume->GetLogicalVolume() != fDetector.ScoringBoxLogical()) return;

  // The box index is the copy number of the box's mother, its DNA shell.
  const G4VTouchable* touchable = pre->GetTouchable();
  const G4int copy = touchable->GetCopyNumber(1);
  RecordBoxBuffer& box = fBuffer[copy];
  box.deposit += step->GetTotalEnergyDeposit();
  if (pre->GetStepStatus() != fGeomBoundary) return;

  // Inward crossing: the pre-step point is on the face of the box just entered.
  const G4Track* track = step->GetTrack();
  const G4ThreeVector translation = touchable->GetTranslation();  // global centre of the touched box
  const G4ThreeVector expected = fDetector.BoxCentre(copy);
  if ((translation - expected).mag() > kCentreTolerance)
  {
    std::ostringstream message;
    message << "scoring box copy " << copy << " is at " << translation / um << " um, expected " << expected / um
            << " um: the copy number is not the box index";
    G4Exception("RecordSteppingAction", "PhspRecord001", FatalException, message.str().c_str());
  }
  const G4ThreeVector local = pre->GetPosition() - translation;
  const G4double halfBox = 0.5 * fDetector.Geometry().boxSide;
  if (std::abs(local.x()) > halfBox + kSurfaceTolerance || std::abs(local.y()) > halfBox + kSurfaceTolerance ||
      std::abs(local.z()) > halfBox + kSurfaceTolerance)
  {
    std::ostringstream message;
    message << "inward crossing at " << local / nm << " nm in the frame of box " << copy
            << " is outside the box (half side " << halfBox / nm << " nm)";
    G4Exception("RecordSteppingAction", "PhspRecord002", FatalException, message.str().c_str());
  }

  PhaseSpaceRecord record;
  record.pdg = track->GetDefinition()->GetPDGEncoding();
  if (record.pdg == 0)
  {
    const G4String message = "particle '" + track->GetDefinition()->GetParticleName() +
                             "' has no PDG code; format v1 records need one (pdg != 0)";
    G4Exception("RecordSteppingAction", "PhspRecord003", FatalException, message.c_str());
  }
  record.trackId = track->GetTrackID();
  record.kineticEnergyEv = pre->GetKineticEnergy() / eV;
  record.positionNm = {local.x() / nm, local.y() / nm, local.z() / nm};
  const G4ThreeVector& direction = pre->GetMomentumDirection();  // boxes are not rotated
  record.direction = {direction.x(), direction.y(), direction.z()};
  record.timeNs = pre->GetGlobalTime() / ns;
  record.weight = pre->GetWeight();
  box.records.push_back(record);
}

// ---- Event: one history per primary with at least one record -----------------------

void RecordEventAction::BeginOfEventAction(const G4Event*)
{
  fBuffer.clear();
}

void RecordEventAction::EndOfEventAction(const G4Event* event)
{
  auto* run = static_cast<RecordRun*>(G4RunManager::GetRunManager()->GetNonConstCurrentRun());
  for (auto& [copy, box] : fBuffer)  // copy-number order = (iy, ix) order
  {
    const G4double depositEv = box.deposit / eV;
    if (box.records.empty())
    {
      if (depositEv > 0) run->AddDepositWithoutRecord(depositEv);
      continue;
    }
    G4int ix = 0, iy = 0;
    fDetector.BoxIndices(copy, ix, iy);
    RecordedHistory history;
    history.stage1EventId = static_cast<std::uint64_t>(event->GetEventID());
    history.boxIx = ix;
    history.boxIy = iy;
    history.inBoxDepositEv = depositEv;
    history.records = std::move(box.records);
    run->AddHistory(std::move(history));
  }
  fBuffer.clear();
}

// ---- Run -----------------------------------------------------------------------------

void RecordRunAction::BeginOfRunAction(const G4Run* run)
{
  if (fOutcome == nullptr) return;  // worker
  fStart = std::chrono::steady_clock::now();
  // G4MTRunManager::Initialize runs a BeamOn(0) to start the workers: ignore
  // runs without events.
  if (run->GetNumberOfEventToBeProcessed() > 0 && fOutcome->runsWithEvents > 0)
    G4Exception("RecordRunAction::BeginOfRunAction", "PhspRecord004", FatalException,
                "phsp-record writes one phase-space file per process: the macro may run /run/beamOn only once");
}

void RecordRunAction::EndOfRunAction(const G4Run* run)
{
  if (fOutcome == nullptr || run->GetNumberOfEventToBeProcessed() == 0) return;
  auto* recordRun = static_cast<RecordRun*>(const_cast<G4Run*>(run));
  std::vector<RecordedHistory>& histories = recordRun->Histories();
  std::sort(histories.begin(), histories.end(), [](const RecordedHistory& a, const RecordedHistory& b) {
    return HistoryOrderKey(a.stage1EventId, a.boxIx, a.boxIy) < HistoryOrderKey(b.stage1EventId, b.boxIx, b.boxIy);
  });
  ++fOutcome->runsWithEvents;
  fOutcome->primariesFired = static_cast<std::uint64_t>(run->GetNumberOfEvent());
  fOutcome->histories = std::move(histories);
  fOutcome->depositWithoutRecordEvents = recordRun->DepositWithoutRecordEvents();
  fOutcome->depositWithoutRecordEv = recordRun->DepositWithoutRecordEv();
  fOutcome->wallTimeS = std::chrono::duration<double>(std::chrono::steady_clock::now() - fStart).count();
  fOutcome->threads = G4RunManager::GetRunManager()->GetNumberOfThreads();
}

// ---- Action initialization --------------------------------------------------------------

void RecordActionInitialization::BuildForMaster() const
{
  SetUserAction(new RecordRunAction(&fOutcome));
}

void RecordActionInitialization::Build() const
{
  SetUserAction(new RecordPrimaryGeneratorAction());
  SetUserAction(new RecordRunAction(nullptr));
  auto* eventAction = new RecordEventAction(fDetector);
  SetUserAction(eventAction);
  SetUserAction(new RecordSteppingAction(fDetector, eventAction->Buffer()));
}

}  // namespace phsp
