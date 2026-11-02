/// Names and reports the selected Geant4 chemistry time-step model.
/// Dependencies: Geant4-dependent; not standard-library-only.
/// Thread safety: Read configuration after initialization; callers coordinate stream output.

#ifndef ChemUtils_h
#define ChemUtils_h 1

#include "G4ChemTimeStepModel.hh"
#include "globals.hh"

namespace ChemUtils
{
    /// Static model label; unrecognized enum values return "Unknown". No failure path.
    const char *ToString(G4ChemTimeStepModel model);

    /// Reads G4EmParameters; use after the desired configuration has been applied.
    G4ChemTimeStepModel GetCurrentTimeStepModel();

    /// Current model label using ToString; no model validation or configuration changes.
    G4String GetCurrentTimeStepModelName();

    /// Writes prefix and the current model to the existing G4cout stream.
    /// Caller coordinates output; stream failures are not reported separately.
    void PrintCurrentTimeStepModel(const G4String &prefix = "[ChemUtils]");
}

#endif // ChemUtils_h