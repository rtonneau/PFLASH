/// \file RootTableFile.cc
/// \brief Write OutputTables as one ROOT file (one TTree ntuple per table).
/// Standard library, OutputTable and g4tools (tools::wroot) only.
///
/// Portable: see RootTableFile.hh for the layout.

#include "scoring/RootTableFile.hh"

#include "tools/wroot/file"
#include "tools/wroot/ntuple"
#include "toolx/zlib"

#include <cstdint>
#include <exception>
#include <memory>
#include <sstream>
#include <variant>

namespace
{
  constexpr std::int64_t kMaxExactInt64 = std::int64_t(1) << 53;

  std::string LeafTitle(const OutputTable::Column& column)
  {
    return column.unit.empty() ? column.name : column.name + " [" + column.unit + "]";
  }

  bool WriteTable(tools::wroot::file& file, const OutputTable& table, std::string& err)
  {
    // The directory owns the ntuple (tree with a_managed = true) and deletes it.
    auto* ntuple = new tools::wroot::ntuple(file.dir(), table.Name(), table.Description());
    const auto& columns = table.Columns();

    for (const auto& column : columns) {
      bool created = false;
      switch (column.type) {
        case OutputTable::ColumnType::Int64:
          // g4tools' wroot::ntuple has no 64-bit integer leaf (only char, short,
          // int, float, double, string): int64 is stored as a double.
          created = ntuple->create_column<double>(column.name) != nullptr;
          break;
        case OutputTable::ColumnType::Double:
          created = ntuple->create_column<double>(column.name) != nullptr;
          break;
        case OutputTable::ColumnType::String:
          created = ntuple->create_column_string(column.name) != nullptr;
          break;
      }
      if (!created) {
        err = "cannot create column " + column.name + " of table " + table.Name();
        return false;
      }
    }

    // Keep the unit in the leaf title.
    for (const auto* branch : ntuple->branches()) {
      for (auto* leaf : branch->leaves()) {
        for (const auto& column : columns) {
          if (leaf->name() == column.name) leaf->set_title(LeafTitle(column));
        }
      }
    }

    for (const auto& row : table.Rows()) {
      for (std::size_t i = 0; i < columns.size(); ++i) {
        const std::string& name = columns[i].name;
        switch (columns[i].type) {
          case OutputTable::ColumnType::Int64:
          {
            const std::int64_t value = std::get<std::int64_t>(row[i]);
            if (value > kMaxExactInt64 || value < -kMaxExactInt64) {
              err = "int64 value of column " + name + " in table " + table.Name() +
                    " exceeds 2^53 and cannot be stored exactly as a ROOT double";
              return false;
            }
            ntuple->find_column<double>(name)->fill(static_cast<double>(value));
            break;
          }
          case OutputTable::ColumnType::Double:
            ntuple->find_column<double>(name)->fill(std::get<double>(row[i]));
            break;
          case OutputTable::ColumnType::String:
            ntuple->find_column_string(name)->fill(std::get<std::string>(row[i]));
            break;
        }
      }
      if (!ntuple->add_row()) {
        err = "cannot add a row to table " + table.Name();
        return false;
      }
    }
    return true;
  }
}

bool RootTableFile::Write(const std::string& path, const std::vector<OutputTable>& tables,
                          std::string& err)
{
  try {
    std::ostringstream messages;
    tools::wroot::file file(messages, path);
    if (!file.is_open()) {
      err = "ROOT table file " + path + ": cannot create the file " + messages.str();
      return false;
    }
    file.add_ziper('Z', toolx::compress_buffer);
    file.set_compression(1);

    for (const OutputTable& table : tables) {
      std::string tableErr;
      if (!WriteTable(file, table, tableErr)) {
        err = "ROOT table file " + path + ": " + tableErr + " " + messages.str();
        return false;
      }
    }
    unsigned int nbytes = 0;
    if (!file.write(nbytes)) {
      err = "ROOT table file " + path + ": cannot write the file " + messages.str();
      return false;
    }
    file.close();
    return true;
  }
  catch (const std::exception& e) {
    err = "ROOT table file " + path + ": " + e.what();
  }
  return false;
}
