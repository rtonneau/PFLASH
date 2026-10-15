/// \file MesoMessenger.hh
/// \brief PreInit macro commands for the mesoscopic stage (new /chem/meso/ directory).
///   /chem/meso/handOverTime <value> <unit>   particle-based -> mesoscopic switch (default 5 ns)
///   /chem/meso/voxelSize <value> <unit>      target initial cell size (default 6.25 nm)
///   /chem/meso/timesPerDecade <int>          mesoscopic record times per decade (default 10)
///   /chem/meso/spatialOutput <bool>          write Spatial snapshots to SpeciesMesoSpatial.h5 (default false)
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

class MesoMessenger : public G4UImessenger
{
public:
  MesoMessenger();
  ~MesoMessenger() override;

  void SetNewValue(G4UIcommand* command, G4String newValue) override;
  G4String GetCurrentValue(G4UIcommand* command) override;

private:
  G4UIcmdWithADoubleAndUnit* fpHandOverCmd;
  G4UIcmdWithADoubleAndUnit* fpVoxelCmd;
  G4UIcmdWithAnInteger* fpPerDecadeCmd;
  G4UIcmdWithABool* fpSpatialOutputCmd;
  G4UIcmdWithABool* fpEnableCmd;
};

#endif  // MesoMessenger_h
