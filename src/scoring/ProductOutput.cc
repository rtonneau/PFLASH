/// \file ProductOutput.cc
/// \brief Write the tables of one Output product in every chosen format.

#include "scoring/ProductOutput.hh"

#include "core/OutputDir.hh"
#include "scoring/Hdf5TableFile.hh"
#include "scoring/RootTableFile.hh"
#include "scoring/TableWriters.hh"

#include "G4Exception.hh"

#include <cctype>
#include <fstream>

namespace
{
  std::string Capitalised(std::string text)
  {
    if (!text.empty())
      text[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(text[0])));
    return text;
  }

  // File-name suffix of a further table: its capitalised name, minus the
  // product's singular when the table is named after it (table
  // "reactionCatalog" of product "Reactions" -> "Catalog", so the file is
  // ReactionsCatalog.csv, not ReactionsReactionCatalog.csv).
  std::string TableSuffix(const std::string &stem, const std::string &tableName)
  {
    const std::string suffix = Capitalised(tableName);
    if (stem.size() > 1 && stem.back() == 's')
    {
      const std::string singular = stem.substr(0, stem.size() - 1);
      if (suffix.size() > singular.size() && suffix.compare(0, singular.size(), singular) == 0)
        return suffix.substr(singular.size());
    }
    return suffix;
  }

  void Warn(const std::string &fileName, const std::string &why)
  {
    const std::string message = "Cannot write " + fileName + ": " + why;
    G4Exception("ProductOutput::Write", "ProductOutputWriteFailed", JustWarning, message.c_str());
  }

  void Record(const std::string &name, const G4String &prefix, std::vector<std::string> &files)
  {
    files.push_back(std::string(prefix) + name);
  }

  void WriteStreamFile(const std::string &name, const G4String &prefix,
                       std::vector<std::string> &files, OutputFormats::Format format,
                       const OutputTable &table)
  {
    const std::string path = OutputDir::Resolve(name);
    std::ofstream out(path);
    if (!out)
    {
      Warn(path, "cannot open for writing");
      return;
    }
    if (format == OutputFormats::Format::Csv)
      TableWriters::WriteCsv(out, table);
    else
      TableWriters::WriteTxt(out, table, table.Description());
    out.close();
    if (!out)
    {
      Warn(path, "write error");
      return;
    }
    Record(name, prefix, files);
  }
}

void ProductOutput::Write(OutputFormats::Product product, const std::vector<OutputTable> &tables,
                          const G4String &prefix, std::vector<std::string> &files)
{
  if (tables.empty())
    return;
  const std::string stem = OutputFormats::FileStem(product);
  for (OutputFormats::Format format : OutputFormats::Effective(product))
  {
    const std::string extension = OutputFormats::Extension(format);
    if (format == OutputFormats::Format::H5 || format == OutputFormats::Format::Root)
    {
      const std::string name = stem + "." + extension;
      const std::string path = OutputDir::Resolve(name);
      std::string err;
      const bool ok = (format == OutputFormats::Format::H5)
                          ? Hdf5TableFile::Write(path, tables, err)
                          : RootTableFile::Write(path, tables, err);
      if (ok)
        Record(name, prefix, files);
      else
        Warn(path, err);
      continue;
    }
    for (std::size_t i = 0; i < tables.size(); ++i)
    {
      const std::string name =
          stem + (i == 0 ? "" : TableSuffix(stem, tables[i].Name())) + "." + extension;
      WriteStreamFile(name, prefix, files, format, tables[i]);
    }
  }
}
