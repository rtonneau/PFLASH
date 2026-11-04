/// \file OutputFormatsMessenger.hh
/// \brief PreInit macro commands choosing the output file formats.
/// Requires Geant4 UI runtime and OutputFormats; not standard-library-only.
/// Thread safety: master-only PreInit writes, before any worker starts.
///
///   /output/defaultFormat <fmt...>        formats for products without their own setting
///   /output/<product>/format <fmt...>     formats of one product (species, speciesMeso,
///                                         reactions, physicsInteractions, trackLengths,
///                                         preChemical, speciesMesoSpatial)
/// <fmt...> is a space-separated list of txt, csv, h5, root, or `none`. Any registry
/// error is a fatal G4Exception. A plain G4UImessenger whose single string parameter
/// takes the rest of the command line (G4GenericMessenger::DeclareMethod would
/// truncate a multi-token value). The /output/ directory is created by these commands.
#ifndef OutputFormatsMessenger_hh
#define OutputFormatsMessenger_hh 1

#include "scoring/OutputFormats.hh"

#include "G4UImessenger.hh"

#include <vector>

class G4UIcmdWithAString;

/// Owns the /output/ commands described above; construct while the Geant4 UI is available.
class OutputFormatsMessenger : public G4UImessenger
{
public:
  /// Registers the commands and tells OutputFormats whether HDF5 is available.
  OutputFormatsMessenger();
  /// Deletes the owned commands; the UI must no longer dispatch to this instance.
  ~OutputFormatsMessenger() override;

  /// Applies a format list; any registry error raises FatalException.
  void SetNewValue(G4UIcommand *command, G4String newValue) override;
  /// Returns the current format list of the command, `none` when empty.
  G4String GetCurrentValue(G4UIcommand *command) override;

private:
  G4UIcmdWithAString *fpDefaultCmd;
  std::vector<G4UIcmdWithAString *> fProductCmds; ///< indexed like OutputFormats::AllProducts()
};

#endif // OutputFormatsMessenger_hh
