#include "CaptureDetectorConstruction.hh"

#include "G4Box.hh"
#include "G4Exception.hh"
#include "G4LogicalVolume.hh"
#include "G4NistManager.hh"
#include "G4PVPlacement.hh"
#include "G4SystemOfUnits.hh"
#include "G4UIcmdWithADoubleAndUnit.hh"
#include "G4UIdirectory.hh"
#include "G4UImessenger.hh"

namespace drs
{

class CaptureDetectorConstruction::Messenger : public G4UImessenger
{
 public:
  explicit Messenger(CaptureDetectorConstruction* detector) : fDetector(detector)
  {
    fDirectory = std::make_unique<G4UIdirectory>("/drs/capture/");
    fDirectory->SetGuidance("Capture geometry of the dose-rate-study example.");
    fBoxCmd = std::make_unique<G4UIcmdWithADoubleAndUnit>("/drs/capture/boxHalfSide", this);
    fBoxCmd->SetGuidance("Half side of the water capture box, centred in the world.");
    fBoxCmd->SetParameterName("boxHalfSide", false);
    fBoxCmd->SetRange("boxHalfSide > 0");
    fBoxCmd->SetUnitCategory("Length");
    fBoxCmd->AvailableForStates(G4State_PreInit);
    fWorldCmd = std::make_unique<G4UIcmdWithADoubleAndUnit>("/drs/capture/worldHalfSide", this);
    fWorldCmd->SetGuidance("Half side of the water world around the capture box.");
    fWorldCmd->SetParameterName("worldHalfSide", false);
    fWorldCmd->SetRange("worldHalfSide > 0");
    fWorldCmd->SetUnitCategory("Length");
    fWorldCmd->AvailableForStates(G4State_PreInit);
  }

  void SetNewValue(G4UIcommand* command, G4String value) override
  {
    if (command == fBoxCmd.get())
      fDetector->SetBoxHalfSide(G4UIcmdWithADoubleAndUnit::GetNewDoubleValue(value));
    else if (command == fWorldCmd.get())
      fDetector->SetWorldHalfSide(G4UIcmdWithADoubleAndUnit::GetNewDoubleValue(value));
  }

 private:
  CaptureDetectorConstruction* fDetector;
  std::unique_ptr<G4UIdirectory> fDirectory;
  std::unique_ptr<G4UIcmdWithADoubleAndUnit> fBoxCmd;
  std::unique_ptr<G4UIcmdWithADoubleAndUnit> fWorldCmd;
};

CaptureDetectorConstruction::CaptureDetectorConstruction()
  : fBoxHalfSide(2 * um), fWorldHalfSide(10 * um), fMessenger(std::make_unique<Messenger>(this))
{}

CaptureDetectorConstruction::~CaptureDetectorConstruction() = default;

void CaptureDetectorConstruction::SetBoxHalfSide(G4double halfSide)
{
  if (!(halfSide > 0))
  {
    G4Exception("CaptureDetectorConstruction::SetBoxHalfSide", "DRS_CAPTURE_GEOMETRY",
                FatalErrorInArgument, "the capture box half side must be positive");
    return;
  }
  fBoxHalfSide = halfSide;
}

void CaptureDetectorConstruction::SetWorldHalfSide(G4double halfSide)
{
  if (!(halfSide > 0))
  {
    G4Exception("CaptureDetectorConstruction::SetWorldHalfSide", "DRS_CAPTURE_GEOMETRY",
                FatalErrorInArgument, "the world half side must be positive");
    return;
  }
  fWorldHalfSide = halfSide;
}

G4VPhysicalVolume* CaptureDetectorConstruction::Construct()
{
  // The primary starts 1 nm inside the -z world face, so the world must leave
  // room upstream of the box (checked here, once both sizes are final).
  if (!(fWorldHalfSide > fBoxHalfSide + 1 * nm))
    G4Exception("CaptureDetectorConstruction::Construct", "DRS_CAPTURE_GEOMETRY",
                FatalErrorInArgument, "the world half side must exceed the capture box half side");

  auto* water = G4NistManager::Instance()->FindOrBuildMaterial("G4_WATER");
  auto* worldSolid = new G4Box("World", fWorldHalfSide, fWorldHalfSide, fWorldHalfSide);
  auto* worldLogical = new G4LogicalVolume(worldSolid, water, "World");
  auto* world = new G4PVPlacement(nullptr, {}, worldLogical, "World", nullptr, false, 0, true);

  auto* boxSolid = new G4Box("CaptureBox", fBoxHalfSide, fBoxHalfSide, fBoxHalfSide);
  auto* boxLogical = new G4LogicalVolume(boxSolid, water, "CaptureBox");
  fBoxVolume = new G4PVPlacement(nullptr, {}, boxLogical, "CaptureBox", worldLogical, false, 0, true);
  return world;
}

}  // namespace drs
