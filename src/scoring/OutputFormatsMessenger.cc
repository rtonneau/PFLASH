/// \file OutputFormatsMessenger.cc
/// \brief PreInit macro commands choosing the output file formats.
/// Requires Geant4 UI runtime and OutputFormats; not standard-library-only.
/// Thread safety: master-only PreInit writes, before any worker starts.

#include "scoring/OutputFormatsMessenger.hh"

#include "scoring/MesoSpatialFile.hh"

#include "G4ApplicationState.hh"
#include "G4Exception.hh"
#include "G4UIcmdWithAString.hh"

#include <string>

namespace
{
  std::string FormatListText(const std::vector<OutputFormats::Format> &formats)
  {
    if (formats.empty())
      return "none";
    std::string text;
    for (OutputFormats::Format format : formats)
      text += (text.empty() ? "" : " ") + OutputFormats::FormatName(format);
    return text;
  }
} // namespace

OutputFormatsMessenger::OutputFormatsMessenger()
    : G4UImessenger()
{
  OutputFormats::SetHdf5Available(MesoSpatialFile::Available());

  fpDefaultCmd = new G4UIcmdWithAString("/output/defaultFormat", this);
  fpDefaultCmd->SetGuidance(
      "Default output file formats for every product without its own "
      "/output/<product>/format. Space-separated list of txt, csv, h5, root, or 'none'. "
      "Built-in default: csv. A product skips the formats it does not accept; "
      "speciesMesoSpatial never follows the default. 'h5' needs an HDF5 build. "
      "Issue before /run/initialize.");
  fpDefaultCmd->SetParameterName("formats", false);
  fpDefaultCmd->AvailableForStates(G4State_PreInit);

  for (OutputFormats::Product product : OutputFormats::AllProducts())
  {
    auto *command = new G4UIcmdWithAString(
        ("/output/" + OutputFormats::CommandName(product) + "/format").c_str(), this);
    command->SetGuidance(("Output file formats of the " + OutputFormats::FileStem(product) +
                          " product, replacing the default. Space-separated list of: " +
                          OutputFormats::AcceptedFormatsText(product) +
                          ", or 'none' for no file. 'h5' needs an HDF5 build. "
                          "Issue before /run/initialize.")
                             .c_str());
    command->SetParameterName("formats", false);
    command->AvailableForStates(G4State_PreInit);
    fProductCmds.push_back(command);
  }
}

OutputFormatsMessenger::~OutputFormatsMessenger()
{
  delete fpDefaultCmd;
  for (G4UIcmdWithAString *command : fProductCmds)
    delete command;
}

void OutputFormatsMessenger::SetNewValue(G4UIcommand *command, G4String newValue)
{
  std::string err;
  bool ok = true;
  if (command == fpDefaultCmd)
  {
    ok = OutputFormats::SetDefault(newValue, err);
  }
  else
  {
    const std::vector<OutputFormats::Product> &products = OutputFormats::AllProducts();
    for (std::size_t i = 0; i < fProductCmds.size(); ++i)
    {
      if (command == fProductCmds[i])
      {
        ok = OutputFormats::SetProduct(products[i], newValue, err);
        break;
      }
    }
  }
  if (!ok)
  {
    G4Exception("OutputFormatsMessenger::SetNewValue", "InvalidOutputFormat", FatalException,
                err.c_str());
  }
}

G4String OutputFormatsMessenger::GetCurrentValue(G4UIcommand *command)
{
  const std::vector<OutputFormats::Product> &products = OutputFormats::AllProducts();
  for (std::size_t i = 0; i < fProductCmds.size(); ++i)
  {
    if (command == fProductCmds[i])
      return FormatListText(OutputFormats::Effective(products[i]));
  }
  return "";
}
