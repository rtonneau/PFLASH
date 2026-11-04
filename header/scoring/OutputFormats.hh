/// \file OutputFormats.hh
/// \brief Registry of the output products and the file formats chosen for each.
/// Dependencies: standard library only.
/// Thread safety: process-wide state, written only at PreInit on the master before
/// any worker starts (like MesoSettings::Current()); concurrent reads are safe.
///
/// Portable and project-agnostic (no Geant4, no dependency on other dnachem-min
/// classes; adjust the include path prefix on copy). Knows the Output products,
/// their macro command names and file stems, and which formats each accepts.
/// A format list is chosen per product, or inherited from the default format
/// list; `none` selects no file at all. Errors are reported as bool + message,
/// the Geant4 messenger raises the G4Exception.
#ifndef OutputFormats_hh
#define OutputFormats_hh 1

#include <string>
#include <vector>

namespace OutputFormats
{
  /// File formats a product can be written in.
  enum class Format
  {
    Txt,
    Csv,
    H5,
    Root
  };

  /// The Output products, one table schema each.
  enum class Product
  {
    Species,
    SpeciesMeso,
    Reactions,
    PhysicsInteractions,
    TrackLengths,
    PreChemical,
    SpeciesMesoSpatial
  };

  /// All products, in declaration order.
  const std::vector<Product> &AllProducts();
  /// All formats, in declaration order (txt, csv, h5, root).
  const std::vector<Format> &AllFormats();

  /// Macro command name of the product (`species`, `speciesMeso`, `reactions`,
  /// `physicsInteractions`, `trackLengths`, `preChemical`, `speciesMesoSpatial`).
  std::string CommandName(Product product);
  /// File stem of the product (`Species`, `SpeciesMeso`, `Reactions`,
  /// `PhysicsInteractions`, `TrackLengths`, `PreChemical`, `SpeciesMesoSpatial`).
  std::string FileStem(Product product);
  /// Lower-case macro name of the format (`txt`, `csv`, `h5`, `root`).
  std::string FormatName(Format format);
  /// File extension of the format, without the dot (`txt`, `csv`, `h5`, `root`).
  std::string Extension(Format format);

  /// Formats the product can be written in (SpeciesMesoSpatial: h5, root; others:
  /// txt, csv, h5, root). Independent of HDF5 availability.
  std::vector<Format> AcceptedFormats(Product product);
  /// Space-separated accepted format names, for guidance and error messages.
  std::string AcceptedFormatsText(Product product);

  /// Tells the registry whether HDF5 output can be written (the messenger passes
  /// MesoSpatialFile::Available()). While false, `h5` is refused by SetDefault and
  /// SetProduct and never appears in Effective. Defaults to true.
  void SetHdf5Available(bool available);

  /// Parses a space-separated list of format names (case-insensitive) or `none`
  /// and stores it as the default for products without their own setting.
  /// Returns false and fills `err` (leaving the state unchanged) for an empty
  /// list, an unknown format, `none` together with another format, or `h5`
  /// while HDF5 is unavailable. Duplicates collapse; a repeat replaces the
  /// previous value. A product that does not accept a listed format simply
  /// skips it in Effective.
  bool SetDefault(const std::string &tokens, std::string &err);

  /// Same as SetDefault for one product's own setting, which takes priority over
  /// the default. Also returns false for a format the product does not accept.
  bool SetProduct(Product product, const std::string &tokens, std::string &err);

  /// Formats to write for the product, in the order txt, csv, h5, root: its own
  /// setting if set, else the default minus the formats it does not accept (and
  /// minus h5 while HDF5 is unavailable). SpeciesMesoSpatial never follows the
  /// default: it is empty unless set. The built-in default is `csv`; `none`
  /// gives an empty list.
  std::vector<Format> Effective(Product product);

  /// Restores the built-in state (default `csv`, no product setting, HDF5 available).
  void ResetForTest();
} // namespace OutputFormats

#endif // OutputFormats_hh
