#include "chemistry/ChemUtils.hh"
#include "G4EmParameters.hh"

namespace ChemUtils
{
    const char *ToString(G4ChemTimeStepModel model)
    {
        switch (model)
        {
        case G4ChemTimeStepModel::SBS:
            return "SBS";
        case G4ChemTimeStepModel::IRT:
            return "IRT";
        case G4ChemTimeStepModel::IRT_syn:
            return "IRT_syn";
        case G4ChemTimeStepModel::Unknown:
        default:
            return "Unknown";
        }
    }

    G4ChemTimeStepModel GetCurrentTimeStepModel()
    {
        return G4EmParameters::Instance()->GetTimeStepModel();
    }

    G4String GetCurrentTimeStepModelName()
    {
        return ToString(GetCurrentTimeStepModel());
    }

    void PrintCurrentTimeStepModel(const G4String &prefix)
    {
        G4cout << prefix << " Chemistry TimeStepModel = "
               << GetCurrentTimeStepModelName() << G4endl;
    }
}