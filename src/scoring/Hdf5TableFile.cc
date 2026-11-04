/// \file Hdf5TableFile.cc
/// \brief Write OutputTables as one HDF5 file.
/// Standard library, OutputTable and HDF5 C++ only.
///
/// Portable: see Hdf5TableFile.hh for the layout.

#include "scoring/Hdf5TableFile.hh"

#include <H5Cpp.h>

#include <cstdint>
#include <exception>
#include <variant>

namespace
{
  H5::StrType Utf8String()
  {
    H5::StrType type(H5::PredType::C_S1, H5T_VARIABLE);
    type.setCset(H5T_CSET_UTF8);
    return type;
  }

  void WriteStringAttr(H5::H5Object& obj, const char* name, const std::string& value)
  {
    H5::StrType type = Utf8String();
    H5::DataSpace scalar(H5S_SCALAR);
    H5::Attribute attr = obj.createAttribute(name, type, scalar);
    const char* ptr = value.c_str();
    attr.write(type, &ptr);
  }

  void WriteColumn(H5::Group& group, const OutputTable& table, std::size_t index)
  {
    const OutputTable::Column& column = table.Columns()[index];
    const std::vector<OutputTable::Row>& rows = table.Rows();
    const hsize_t n = rows.size();
    H5::DataSpace space(1, &n);
    H5::DataSet dataSet;

    switch (column.type) {
      case OutputTable::ColumnType::Int64: {
        std::vector<std::int64_t> values;
        values.reserve(rows.size());
        for (const auto& row : rows) values.push_back(std::get<std::int64_t>(row[index]));
        dataSet = group.createDataSet(column.name, H5::PredType::NATIVE_INT64, space);
        if (n > 0) dataSet.write(values.data(), H5::PredType::NATIVE_INT64);
        break;
      }
      case OutputTable::ColumnType::Double: {
        std::vector<double> values;
        values.reserve(rows.size());
        for (const auto& row : rows) values.push_back(std::get<double>(row[index]));
        dataSet = group.createDataSet(column.name, H5::PredType::NATIVE_DOUBLE, space);
        if (n > 0) dataSet.write(values.data(), H5::PredType::NATIVE_DOUBLE);
        break;
      }
      case OutputTable::ColumnType::String: {
        std::vector<const char*> values;
        values.reserve(rows.size());
        for (const auto& row : rows) values.push_back(std::get<std::string>(row[index]).c_str());
        H5::StrType type = Utf8String();
        dataSet = group.createDataSet(column.name, type, space);
        if (n > 0) dataSet.write(values.data(), type);
        break;
      }
    }
    WriteStringAttr(dataSet, "unit", column.unit);
    WriteStringAttr(dataSet, "description", column.description);
  }
}

bool Hdf5TableFile::Available()
{
  return true;
}

bool Hdf5TableFile::Write(const std::string& path, const std::vector<OutputTable>& tables,
                          std::string& err)
{
  try {
    H5::Exception::dontPrint();
    H5::H5File file(path, H5F_ACC_TRUNC);
    H5::DataSpace scalar(H5S_SCALAR);
    H5::Attribute version =
      file.createAttribute("formatVersion", H5::PredType::NATIVE_INT, scalar);
    const int formatVersion = kFormatVersion;
    version.write(H5::PredType::NATIVE_INT, &formatVersion);

    for (const OutputTable& table : tables) {
      H5::Group group = file.createGroup("/" + table.Name());
      WriteStringAttr(group, "description", table.Description());
      for (std::size_t i = 0; i < table.Columns().size(); ++i) WriteColumn(group, table, i);
    }
    return true;
  }
  catch (const H5::Exception& e) {
    err = "HDF5 table file " + path + ": " + e.getDetailMsg();
  }
  catch (const std::exception& e) {
    err = "HDF5 table file " + path + ": " + e.what();
  }
  return false;
}
