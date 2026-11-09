#include "Ledger.hh"

#include <cmath>
#include <cstdio>
#include <ostream>
#include <stdexcept>

namespace ptirt
{
namespace
{

std::string FormatReal(double x)
{
  char buffer[40];
  std::snprintf(buffer, sizeof buffer, "%.17g", x);
  return buffer;
}

std::string CsvReal(double x) { return std::isfinite(x) ? FormatReal(x) : std::string(); }

std::string CsvText(const std::string& text)
{
  if (text.find_first_of(",\"\r\n") == std::string::npos) return text;
  std::string quoted = "\"";
  for (char c : text)
  {
    if (c == '"') quoted += '"';
    quoted += c;
  }
  return quoted + '"';
}

bool IsScalar(const JsonValue& v)
{
  return v.GetType() != JsonValue::Type::Array && v.GetType() != JsonValue::Type::Object;
}

void WriteScalar(std::ostream& out, const JsonValue& v)
{
  switch (v.GetType())
  {
    case JsonValue::Type::Null: out << "null"; break;
    case JsonValue::Type::Bool: out << (v.BoolValue() ? "true" : "false"); break;
    case JsonValue::Type::Integer: out << v.IntegerValue(); break;
    case JsonValue::Type::Real: out << (std::isfinite(v.RealValue()) ? FormatReal(v.RealValue()) : "null"); break;
    case JsonValue::Type::String: out << JsonQuote(v.StringValue()); break;
    default: break;
  }
}

void WriteNode(std::ostream& out, const JsonValue& v, int depth)
{
  const std::string pad(static_cast<std::size_t>(depth) * 2, ' ');
  const std::string inner(static_cast<std::size_t>(depth + 1) * 2, ' ');
  if (v.GetType() == JsonValue::Type::Array)
  {
    bool allScalar = true;
    for (const auto& e : v.Elements()) allScalar = allScalar && IsScalar(e);
    if (v.Elements().empty()) { out << "[]"; return; }
    if (allScalar)
    {
      out << '[';
      for (std::size_t i = 0; i < v.Elements().size(); ++i)
      {
        if (i) out << ", ";
        WriteScalar(out, v.Elements()[i]);
      }
      out << ']';
      return;
    }
    out << "[\n";
    for (std::size_t i = 0; i < v.Elements().size(); ++i)
    {
      out << inner;
      WriteNode(out, v.Elements()[i], depth + 1);
      out << (i + 1 < v.Elements().size() ? ",\n" : "\n");
    }
    out << pad << ']';
  }
  else if (v.GetType() == JsonValue::Type::Object)
  {
    if (v.Members().empty()) { out << "{}"; return; }
    bool allScalar = true;
    for (const auto& m : v.Members()) allScalar = allScalar && IsScalar(m.second);
    if (allScalar)
    {
      out << '{';
      for (std::size_t i = 0; i < v.Members().size(); ++i)
      {
        if (i) out << ", ";
        out << JsonQuote(v.Members()[i].first) << ": ";
        WriteScalar(out, v.Members()[i].second);
      }
      out << '}';
      return;
    }
    out << "{\n";
    for (std::size_t i = 0; i < v.Members().size(); ++i)
    {
      out << inner << JsonQuote(v.Members()[i].first) << ": ";
      WriteNode(out, v.Members()[i].second, depth + 1);
      out << (i + 1 < v.Members().size() ? ",\n" : "\n");
    }
    out << pad << '}';
  }
  else
  {
    WriteScalar(out, v);
  }
}

}  // namespace

void WriteActivationsCsv(std::ostream& out, const std::vector<ActivationRecord>& records)
{
  out << "sourceIndex,packetIndex,offsetXNm,offsetYNm,offsetZNm,scheduledTimeNs,"
         "expectedActivationTimeNs,observedActivationTimeNs,firstTrackId,lastTrackId,"
         "moleculeCount,energyDepositEv,escapedEnergyEv\n";
  for (const auto& r : records)
  {
    out << r.sourceIndex << ',' << r.packetIndex << ',' << CsvReal(r.offsetNm[0]) << ','
        << CsvReal(r.offsetNm[1]) << ',' << CsvReal(r.offsetNm[2]) << ',' << CsvReal(r.scheduledTimeNs)
        << ',' << CsvReal(r.expectedActivationTimeNs) << ',' << CsvReal(r.observedActivationTimeNs) << ','
        << r.firstTrackId << ',' << r.lastTrackId << ',' << r.moleculeCount << ','
        << CsvReal(r.energyDepositEv) << ',' << CsvReal(r.escapedEnergyEv) << '\n';
  }
  if (!out) throw std::runtime_error("Ledger: activations stream failed");
}

void WriteSpeciesCsv(std::ostream& out, const std::vector<SpeciesRow>& rows, bool includeHistoryColumns)
{
  out << "timeNs,species,count";
  if (includeHistoryColumns) out << ",history,chemistrySeed";
  out << '\n';
  for (const auto& r : rows)
  {
    out << CsvReal(r.timeNs) << ',' << CsvText(r.species) << ',' << CsvReal(r.count);
    if (includeHistoryColumns) out << ',' << CsvText(r.history) << ',' << r.chemistrySeed;
    out << '\n';
  }
  if (!out) throw std::runtime_error("Ledger: species stream failed");
}

JsonValue JsonValue::Bool(bool value)
{
  JsonValue v;
  v.type_ = Type::Bool;
  v.bool_ = value;
  return v;
}
JsonValue JsonValue::Integer(long long value)
{
  JsonValue v;
  v.type_ = Type::Integer;
  v.integer_ = value;
  return v;
}
JsonValue JsonValue::Real(double value)
{
  JsonValue v;
  v.type_ = Type::Real;
  v.real_ = value;
  return v;
}
JsonValue JsonValue::String(std::string value)
{
  JsonValue v;
  v.type_ = Type::String;
  v.string_ = std::move(value);
  return v;
}
JsonValue JsonValue::Array()
{
  JsonValue v;
  v.type_ = Type::Array;
  return v;
}
JsonValue JsonValue::Object()
{
  JsonValue v;
  v.type_ = Type::Object;
  return v;
}

JsonValue& JsonValue::Set(const std::string& key, JsonValue value)
{
  if (type_ != Type::Object) throw std::logic_error("JsonValue::Set on a non-object");
  for (auto& member : members_)
    if (member.first == key)
    {
      member.second = std::move(value);
      return *this;
    }
  members_.emplace_back(key, std::move(value));
  return *this;
}

JsonValue& JsonValue::Append(JsonValue value)
{
  if (type_ != Type::Array) throw std::logic_error("JsonValue::Append on a non-array");
  elements_.push_back(std::move(value));
  return *this;
}

std::string JsonQuote(const std::string& text)
{
  std::string quoted = "\"";
  for (unsigned char c : text)
  {
    switch (c)
    {
      case '"': quoted += "\\\""; break;
      case '\\': quoted += "\\\\"; break;
      case '\b': quoted += "\\b"; break;
      case '\f': quoted += "\\f"; break;
      case '\n': quoted += "\\n"; break;
      case '\r': quoted += "\\r"; break;
      case '\t': quoted += "\\t"; break;
      default:
        if (c < 0x20)
        {
          char buffer[8];
          std::snprintf(buffer, sizeof buffer, "\\u%04x", c);
          quoted += buffer;
        }
        else
        {
          quoted += static_cast<char>(c);
        }
    }
  }
  return quoted + '"';
}

void WriteJson(std::ostream& out, const JsonValue& value)
{
  WriteNode(out, value, 0);
  out << '\n';
  if (!out) throw std::runtime_error("Ledger: JSON stream failed");
}

}  // namespace ptirt
