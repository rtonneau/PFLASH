#ifndef ChemUtils_h
#define ChemUtils_h 1

#include "G4ChemTimeStepModel.hh"
#include "globals.hh"

// Namespace = clean way to group free functions without polluting global scope
namespace ChemUtils
{
    // Converts the enum to a human-readable string
    const char *ToString(G4ChemTimeStepModel model);

    // Convenience: fetches current model directly from G4EmParameters
    G4ChemTimeStepModel GetCurrentTimeStepModel();

    G4String GetCurrentTimeStepModelName();

    // Convenience: one-liner to print the model, ready to call from anywhere
    void PrintCurrentTimeStepModel(const G4String &prefix = "[ChemUtils]");
}

#endif // ChemUtils_h