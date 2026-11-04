/// \file TableWriters.cc
/// \brief Implementation of the OutputTable csv and txt writers (standard library only).
#include "scoring/TableWriters.hh"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <ostream>
#include <sstream>
#include <vector>

namespace
{
  std::string DoubleFull(double v)
  {
    if (std::isnan(v))
      return "nan";
    if (std::isinf(v))
      return v < 0 ? "-inf" : "inf";
    char buf[64];
    const auto res = std::to_chars(buf, buf + sizeof(buf), v);
    return std::string(buf, res.ptr);
  }

  std::string DoubleShort(double v)
  {
    if (std::isnan(v))
      return "nan";
    if (std::isinf(v))
      return v < 0 ? "-inf" : "inf";
    std::ostringstream os;
    os.precision(6);
    os << v;
    return os.str();
  }

  std::string CsvString(const std::string &s)
  {
    if (s.find_first_of(",\"") == std::string::npos)
      return s;
    std::string q = "\"";
    for (char c : s)
    {
      if (c == '"')
        q += '"';
      q += c;
    }
    q += '"';
    return q;
  }

  // txt string cell: "-" when empty; double-quoted (with "" for an embedded
  // quote) when it contains whitespace or a quote, so a label such as
  // "e_aq + OH -> OHm" stays one field for whitespace-separated readers.
  std::string TxtString(const std::string &s)
  {
    if (s.empty())
      return "-";
    if (s.find_first_of(" \t\"") == std::string::npos)
      return s;
    std::string quoted = "\"";
    for (char ch : s)
      quoted += (ch == '"') ? std::string("\"\"") : std::string(1, ch);
    return quoted + "\"";
  }

  std::string CellText(const OutputTable::Cell &cell, bool full, bool csv)
  {
    if (const auto *i = std::get_if<std::int64_t>(&cell))
      return std::to_string(*i);
    if (const auto *d = std::get_if<double>(&cell))
      return full ? DoubleFull(*d) : DoubleShort(*d);
    const std::string &s = std::get<std::string>(cell);
    if (csv)
      return CsvString(s);
    return TxtString(s);
  }
}

void TableWriters::WriteCsv(std::ostream &out, const OutputTable &table)
{
  const auto &cols = table.Columns();
  for (std::size_t i = 0; i < cols.size(); ++i)
    out << (i ? "," : "") << CsvString(cols[i].name);
  out << '\n';
  for (const auto &row : table.Rows())
  {
    for (std::size_t i = 0; i < row.size(); ++i)
      out << (i ? "," : "") << CellText(row[i], true, true);
    out << '\n';
  }
}

void TableWriters::WriteTxt(std::ostream &out, const OutputTable &table,
                            const std::string &productDescription)
{
  const auto &cols = table.Columns();
  out << "# " << productDescription << '\n';
  out << "# units:";
  for (const auto &c : cols)
    if (!c.unit.empty())
      out << ' ' << c.name << '=' << c.unit;
  out << '\n';

  std::vector<std::size_t> width(cols.size());
  std::vector<std::vector<std::string>> text;
  for (std::size_t i = 0; i < cols.size(); ++i)
    width[i] = cols[i].name.size();
  for (const auto &row : table.Rows())
  {
    std::vector<std::string> line;
    for (std::size_t i = 0; i < row.size(); ++i)
    {
      line.push_back(CellText(row[i], false, false));
      width[i] = std::max(width[i], line.back().size());
    }
    text.push_back(std::move(line));
  }

  auto emit = [&](const std::vector<std::string> &line)
  {
    for (std::size_t i = 0; i < line.size(); ++i)
    {
      if (i)
        out << "  ";
      const std::size_t pad = width[i] - line[i].size();
      if (cols[i].type == OutputTable::ColumnType::String)
        out << line[i] << std::string(pad, ' ');
      else
        out << std::string(pad, ' ') << line[i];
    }
    out << '\n';
  };
  std::vector<std::string> header;
  for (const auto &c : cols)
    header.push_back(c.name);
  emit(header);
  for (const auto &line : text)
    emit(line);
}
