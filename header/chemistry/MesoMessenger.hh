/// \file MesoMessenger.hh
/// Exposes mesoscopic-stage configuration commands.
/// Dependencies: Geant4 UI and MesoSettings; not standard-library-only.
/// Thread safety: Master-only PreInit writes; workers subsequently read the settings.
/// \brief PreInit macro commands for the mesoscopic stage (new /chem/meso/ directory).
///   /chem/meso/handOverTime <value> <unit>   particle-based -> mesoscopic switch (default 5 ns)
///   /chem/meso/voxelSize <value> <unit>      target initial cell size (default 6.25 nm)
///   /chem/meso/timesPerDecade <int>          mesoscopic record times per decade (default 10)
///   /chem/meso/enable <bool>                 run the mesoscopic stage after the hand-over (default true;
///                                            off under SBS, where an explicit true is fatal)
/// Values go to the process-wide MesoSettings::Current(); invalid values are a
/// fatal G4Exception. A plain G4UImessenger: its commands create /chem/meso/
/// under the /chem/ directory made by G4DNAChemistryManager.
#ifndef MesoMessenger_h
#define MesoMessenger_h 1

#include "G4UImessenger.hh"

class G4UIcmdWithADoubleAndUnit;
class G4UIcmdWithAnInteger;
class G4UIcmdWithABool;

/// Owns the UI commands described above; use on the master with the Geant4 UI active.
class MesoMessenger : public G4UImessenger
{
public:
  /// Registers commands after G4DNAChemistryManager creates /chem/.
  MesoMessenger();
  /// Deletes owned commands; the UI must no longer dispatch to this instance.
  ~MesoMessenger() override;

  /// Applies an owned command using Geant4-parsed units (stored as ns/mm).
  /// Nonpositive numeric values raise FatalException; unknown commands do nothing.
  void SetNewValue(G4UIcommand* command, G4String newValue) override;
  /// Returns the owned command's current value; unknown command pointers return empty.
  G4String GetCurrentValue(G4UIcommand* command) override;

private:
  G4UIcmdWithADoubleAndUnit* fpHandOverCmd;
  G4UIcmdWithADoubleAndUnit* fpVoxelCmd;
  G4UIcmdWithAnInteger* fpPerDecadeCmd;
  G4UIcmdWithABool* fpEnableCmd;
};

#endif  // MesoMessenger_h
