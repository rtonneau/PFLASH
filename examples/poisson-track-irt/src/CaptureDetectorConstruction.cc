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

namespace ptirt
{

class CaptureDetectorConstruction::Messenger : public G4UImessenger
{
 public:
  explicit Messenger(CaptureDetectorConstruction* detector) : fDetector(detector)
  {
    fDirectory = std::make_unique<G4UIdirectory>("/ptirt/capture/");
    fDirectory->SetGuidance("Capture volume of the poisson-track-irt example.");
    fHalfSideCmd = std::make_unique<G4UIcmdWithADoubleAndUnit>("/ptirt/capture/halfSide", this);
    fHalfSideCmd->SetGuidance("Half side of the water capture box (the whole world).");
    fHalfSideCmd->SetParameterName("halfSide", false);
    fHalfSideCmd->SetRange("halfSide > 0");
    fHalfSideCmd->SetUnitCategory("Length");
    fHalfSideCmd->AvailableForStates(G4State_PreInit);
  }

  void SetNewValue(G4UIcommand* command, G4String value) override
  {
    if (command == fHalfSideCmd.get())
      fDetector->SetHalfSide(G4UIcmdWithADoubleAndUnit::GetNewDoubleValue(value));
  }

 private:
  CaptureDetectorConstruction* fDetector;
  std::unique_ptr<G4UIdirectory> fDirectory;
  std::unique_ptr<G4UIcmdWithADoubleAndUnit> fHalfSideCmd;
};

CaptureDetectorConstruction::CaptureDetectorConstruction()
  : fHalfSide(10 * um), fMessenger(std::make_unique<Messenger>(this))
{}

CaptureDetectorConstruction::~CaptureDetectorConstruction() = default;

void CaptureDetectorConstruction::SetHalfSide(G4double halfSide)
{
  if (!(halfSide > 0))
  {
    G4Exception("CaptureDetectorConstruction::SetHalfSide", "PTIRT_CAPTURE_HALFSIDE",
                FatalErrorInArgument, "the capture half side must be positive");
    return;
  }
  fHalfSide = halfSide;
}

G4VPhysicalVolume* CaptureDetectorConstruction::Construct()
{
  auto* water = G4NistManager::Instance()->FindOrBuildMaterial("G4_WATER");
  auto* solid = new G4Box("CaptureBox", fHalfSide, fHalfSide, fHalfSide);
  auto* logical = new G4LogicalVolume(solid, water, "CaptureBox");
  return new G4PVPlacement(nullptr, {}, logical, "CaptureBox", nullptr, false, 0, true);
}

}  // namespace ptirt
