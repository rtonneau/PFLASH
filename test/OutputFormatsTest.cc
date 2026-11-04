/// \file OutputFormatsTest.cc
/// Dependencies: Standard library and portable project code; no Geant4 dependency. Optional MSVC
/// CRT diagnostics.
/// Thread safety: Sequential test process; mutates process-wide test state.
/// \brief Plain-assert unit tests for the OutputFormats registry (no Geant4 runtime).
#include "scoring/OutputFormats.hh"

#include <cassert>
#include <iostream>
#include <string>
#include <vector>
#ifdef _MSC_VER
#include <crtdbg.h>
#endif

using OutputFormats::Format;
using OutputFormats::Product;
using FormatList = std::vector<Format>;

namespace
{
  void TestProductTable()
  {
    assert(OutputFormats::AllProducts().size() == 7);
    const struct
    {
      Product product;
      const char *command;
      const char *stem;
    } expected[] = {
        {Product::Species, "species", "Species"},
        {Product::SpeciesMeso, "speciesMeso", "SpeciesMeso"},
        {Product::Reactions, "reactions", "Reactions"},
        {Product::PhysicsInteractions, "physicsInteractions", "PhysicsInteractions"},
        {Product::TrackLengths, "trackLengths", "TrackLengths"},
        {Product::PreChemical, "preChemical", "PreChemical"},
        {Product::SpeciesMesoSpatial, "speciesMesoSpatial", "SpeciesMesoSpatial"},
    };
    for (const auto &row : expected)
    {
      assert(OutputFormats::CommandName(row.product) == row.command);
      assert(OutputFormats::FileStem(row.product) == row.stem);
    }
    assert(OutputFormats::FormatName(Format::Txt) == "txt");
    assert(OutputFormats::FormatName(Format::Csv) == "csv");
    assert(OutputFormats::FormatName(Format::H5) == "h5");
    assert(OutputFormats::FormatName(Format::Root) == "root");
    assert(OutputFormats::Extension(Format::Txt) == "txt");
    assert(OutputFormats::Extension(Format::Csv) == "csv");
    assert(OutputFormats::Extension(Format::H5) == "h5");
    assert(OutputFormats::Extension(Format::Root) == "root");
  }

  void TestAcceptedFormats()
  {
    const FormatList all = {Format::Txt, Format::Csv, Format::H5, Format::Root};
    for (Product product : OutputFormats::AllProducts())
    {
      if (product == Product::SpeciesMesoSpatial)
        assert((OutputFormats::AcceptedFormats(product) == FormatList{Format::H5, Format::Root}));
      else
        assert(OutputFormats::AcceptedFormats(product) == all);
    }
    assert(OutputFormats::AcceptedFormatsText(Product::SpeciesMesoSpatial) == "h5 root");
    assert(OutputFormats::AcceptedFormatsText(Product::Species) == "txt csv h5 root");
  }

  void TestBuiltInDefault()
  {
    OutputFormats::ResetForTest();
    for (Product product : OutputFormats::AllProducts())
    {
      if (product == Product::SpeciesMesoSpatial)
        assert(OutputFormats::Effective(product).empty());
      else
        assert(OutputFormats::Effective(product) == FormatList{Format::Csv});
    }
  }

  void TestSetDefault()
  {
    OutputFormats::ResetForTest();
    std::string err;
    assert(OutputFormats::SetDefault("txt csv", err));
    assert((OutputFormats::Effective(Product::Species) == FormatList{Format::Txt, Format::Csv}));
    assert((OutputFormats::Effective(Product::TrackLengths) == FormatList{Format::Txt, Format::Csv}));
    // SpeciesMesoSpatial never follows the default.
    assert(OutputFormats::Effective(Product::SpeciesMesoSpatial).empty());

    // A repeat replaces the previous value.
    assert(OutputFormats::SetDefault("root", err));
    assert(OutputFormats::Effective(Product::Species) == FormatList{Format::Root});

    // Duplicates collapse; order is canonical; case is ignored.
    assert(OutputFormats::SetDefault("CSV csv  txt csv", err));
    assert((OutputFormats::Effective(Product::Reactions) == FormatList{Format::Txt, Format::Csv}));

    // none -> empty.
    assert(OutputFormats::SetDefault("none", err));
    assert(OutputFormats::Effective(Product::Species).empty());
    assert(OutputFormats::Effective(Product::PreChemical).empty());

    // h5 + root in the default: SpeciesMesoSpatial still empty.
    assert(OutputFormats::SetDefault("h5 root", err));
    assert((OutputFormats::Effective(Product::Species) == FormatList{Format::H5, Format::Root}));
    assert(OutputFormats::Effective(Product::SpeciesMesoSpatial).empty());
  }

  void TestSetDefaultErrors()
  {
    OutputFormats::ResetForTest();
    std::string err;
    assert(OutputFormats::SetDefault("txt", err));

    err.clear();
    assert(!OutputFormats::SetDefault("xml", err));
    assert(err.find("xml") != std::string::npos);

    err.clear();
    assert(!OutputFormats::SetDefault("csv xml", err));
    assert(!err.empty());

    err.clear();
    assert(!OutputFormats::SetDefault("none csv", err));
    assert(err.find("none") != std::string::npos);

    err.clear();
    assert(!OutputFormats::SetDefault("csv none", err));
    assert(!err.empty());

    err.clear();
    assert(!OutputFormats::SetDefault("", err));
    assert(!err.empty());

    err.clear();
    assert(!OutputFormats::SetDefault("   ", err));
    assert(!err.empty());

    // Failed calls leave the previous value untouched.
    assert(OutputFormats::Effective(Product::Species) == FormatList{Format::Txt});
  }

  void TestSetProduct()
  {
    OutputFormats::ResetForTest();
    std::string err;
    assert(OutputFormats::SetDefault("csv", err));

    // Own setting wins over the default.
    assert(OutputFormats::SetProduct(Product::Species, "txt root", err));
    assert((OutputFormats::Effective(Product::Species) == FormatList{Format::Txt, Format::Root}));
    assert(OutputFormats::Effective(Product::Reactions) == FormatList{Format::Csv});

    // A repeat replaces; duplicates collapse.
    assert(OutputFormats::SetProduct(Product::Species, "h5 h5", err));
    assert(OutputFormats::Effective(Product::Species) == FormatList{Format::H5});

    // none is an own setting: empty even though the default is csv.
    assert(OutputFormats::SetProduct(Product::Reactions, "none", err));
    assert(OutputFormats::Effective(Product::Reactions).empty());
    assert(OutputFormats::Effective(Product::TrackLengths) == FormatList{Format::Csv});

    // The spatial product only takes what it is told, and only h5 / root.
    assert(OutputFormats::SetProduct(Product::SpeciesMesoSpatial, "h5", err));
    assert(OutputFormats::Effective(Product::SpeciesMesoSpatial) == FormatList{Format::H5});
    assert(OutputFormats::SetProduct(Product::SpeciesMesoSpatial, "root h5", err));
    assert((OutputFormats::Effective(Product::SpeciesMesoSpatial) ==
            FormatList{Format::H5, Format::Root}));
    assert(OutputFormats::SetProduct(Product::SpeciesMesoSpatial, "none", err));
    assert(OutputFormats::Effective(Product::SpeciesMesoSpatial).empty());
  }

  void TestSetProductErrors()
  {
    OutputFormats::ResetForTest();
    std::string err;

    // A format the product does not accept.
    assert(!OutputFormats::SetProduct(Product::SpeciesMesoSpatial, "csv", err));
    assert(err.find("csv") != std::string::npos);
    assert(err.find("h5 root") != std::string::npos); // guidance lists the accepted formats
    err.clear();
    assert(!OutputFormats::SetProduct(Product::SpeciesMesoSpatial, "h5 txt", err));
    assert(!err.empty());

    // Unknown format.
    err.clear();
    assert(!OutputFormats::SetProduct(Product::Species, "parquet", err));
    assert(err.find("parquet") != std::string::npos);

    // none with another format.
    err.clear();
    assert(!OutputFormats::SetProduct(Product::Species, "none csv", err));
    assert(!err.empty());

    err.clear();
    assert(!OutputFormats::SetProduct(Product::Species, "", err));
    assert(!err.empty());

    // Failed calls leave the product on the default.
    assert(OutputFormats::Effective(Product::Species) == FormatList{Format::Csv});
    assert(OutputFormats::Effective(Product::SpeciesMesoSpatial).empty());
  }

  void TestHdf5Unavailable()
  {
    OutputFormats::ResetForTest();
    OutputFormats::SetHdf5Available(false);
    std::string err;

    assert(!OutputFormats::SetDefault("csv h5", err));
    assert(err.find("HDF5") != std::string::npos);
    err.clear();
    assert(!OutputFormats::SetProduct(Product::Species, "h5", err));
    assert(!err.empty());
    err.clear();
    assert(!OutputFormats::SetProduct(Product::SpeciesMesoSpatial, "h5", err));
    assert(!err.empty());

    // root and csv still work, and the built-in default is intact.
    assert(OutputFormats::SetProduct(Product::SpeciesMesoSpatial, "root", err));
    assert(OutputFormats::Effective(Product::SpeciesMesoSpatial) == FormatList{Format::Root});
    assert(OutputFormats::SetDefault("root", err));
    assert(OutputFormats::Effective(Product::Species) == FormatList{Format::Root});

    // h5 set while available is filtered out of the default once HDF5 goes away.
    OutputFormats::ResetForTest();
    assert(OutputFormats::SetDefault("csv h5", err));
    assert((OutputFormats::Effective(Product::Species) == FormatList{Format::Csv, Format::H5}));
    OutputFormats::SetHdf5Available(false);
    assert(OutputFormats::Effective(Product::Species) == FormatList{Format::Csv});

    OutputFormats::ResetForTest();
    assert(OutputFormats::SetDefault("h5", err)); // available again after the reset
  }

  void TestUnacceptedDefaultFormatsAreSkipped()
  {
    // The default may list a format some product does not accept; Effective skips it
    // (only the spatial product restricts formats, and it never follows the default).
    OutputFormats::ResetForTest();
    std::string err;
    assert(OutputFormats::SetDefault("txt h5", err));
    assert((OutputFormats::Effective(Product::Species) == FormatList{Format::Txt, Format::H5}));
    assert(OutputFormats::Effective(Product::SpeciesMesoSpatial).empty());
  }
} // namespace

int main()
{
#ifdef _MSC_VER
  _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
  _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
  _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
  TestProductTable();
  TestAcceptedFormats();
  TestBuiltInDefault();
  TestSetDefault();
  TestSetDefaultErrors();
  TestSetProduct();
  TestSetProductErrors();
  TestHdf5Unavailable();
  TestUnacceptedDefaultFormatsAreSkipped();
  std::cout << "OutputFormatsTest: all tests passed\n";
  return 0;
}
