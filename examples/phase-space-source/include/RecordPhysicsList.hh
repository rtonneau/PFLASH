// RecordPhysicsList: the stage-1 physics of phsp-record.
//
// G4EmStandardPhysics_option4 everywhere, and DNA_Opt2 (the models of
// G4EmDNAPhysics_option2) in the region "DnaRegion" (the DNA shell around each
// scoring box, and the box), activated with G4EmParameters::AddDNA + the
// G4EmDNAPhysicsActivator constructor, as in the Geant4 example
// dna/microdosimetry.
//
// Checked in the Geant4 11.4.3 source:
// - G4EmStandardPhysics_option4's constructor calls G4EmParameters::SetDefaults,
//   which clears the DNA region list, so AddDNA is called after it.
// - G4EmDNAPhysicsActivator::ConstructProcess (G4EmDNAPhysicsActivator.cc) uses
//   emaxDNA = 1 MeV for e-: in the region the standard e- models (msc,
//   ionisation, bremsstrahlung) are switched on only above 1 MeV, and
//   G4EmDNABuilder::ConstructDNAElectronPhysics with option 2 installs, up to
//   1 MeV, Champion elastic (7.4 eV-1 MeV), Born excitation and ionisation
//   (up to 1 MeV), Sanche vibrational excitation, Melton attachment and
//   solvation (below 7.4 eV). So a 500 keV e- and all its secondaries are on
//   DNA models in the region. Gammas keep the option4 models everywhere.
// - It also switches fluorescence on (SetFluo(true)).
//
// Production cuts: the default cut (outside the region) is kDefaultCut,
// provisional: 1 um, which in water is below the 990 eV lower edge of the cut
// table, so condensed-history delta rays are produced above 990 eV (range in
// water of order 0.05 um, much less than the shell). The region gets the cuts of
// dna/microdosimetry's DNA region (0.7 mm for gamma, e-, e+ and 1 um for
// protons): the DNA models ignore cuts, so they only matter for the standard
// models above 1 MeV. Both can be changed with /run/setCut and
// /run/setCutForRegion DnaRegion.
#pragma once

#include "G4SystemOfUnits.hh"
#include "G4VModularPhysicsList.hh"

class G4Region;

namespace phsp
{

class RecordPhysicsList : public G4VModularPhysicsList
{
 public:
  static constexpr G4double kDefaultCut = 1. * um;
  static constexpr G4double kDnaRegionCut = 0.7 * mm;
  static constexpr G4double kDnaRegionProtonCut = 1. * um;
  static constexpr const char* kDnaOption = "DNA_Opt2";

  RecordPhysicsList();
  ~RecordPhysicsList() override = default;

  // Region cuts for DnaRegion; called by RecordDetectorConstruction.
  static void SetDnaRegionCuts(G4Region* region);
};

}  // namespace phsp
