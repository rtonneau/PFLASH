#include "Ledger.hh"

#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <istream>
#include <iterator>
#include <limits>
#include <ostream>
#include <set>
#include <stdexcept>
#include <utility>

namespace drs
{
namespace
{

void RequireRows(bool ok, const std::string& message)
{
  if (!ok) throw std::invalid_argument("Ledger: " + message);
}

double GPer100Ev(double count, double energyEv)
{
  return energyEv > 0 ? 100.0 * count / energyEv : std::numeric_limits<double>::quiet_NaN();
}

void CheckNormalization(const DoseNormalization& n)
{
  RequireRows(std::isfinite(n.finalEnergyEv) && n.finalEnergyEv > 0, "the final energy must be positive");
  RequireRows(std::isfinite(n.boxMassKg) && n.boxMassKg > 0, "the box mass must be positive");
}

const std::vector<double>* MeasuredAt(const SpeciesTable& measured, double timeNs)
{
  const auto it = measured.countsAt.find(timeNs);
  if (it == measured.countsAt.end()) return nullptr;
  RequireRows(it->second.size() == measured.species.size(), "a measured time lacks a count per species");
  return &it->second;
}

// Strict recursive-descent reader behind ParseJson.
class JsonReader
{
 public:
  explicit JsonReader(std::string text) : fText(std::move(text)) {}

  JsonValue Document()
  {
    auto value = Value(0);
    Blank();
    if (fPos != fText.size()) Fail("trailing characters");
    return value;
  }

 private:
  [[noreturn]] void Fail(const std::string& message) const
  {
    throw std::runtime_error("JSON: " + message + " at offset " + std::to_string(fPos));
  }

  void Blank()
  {
    while (fPos < fText.size() &&
           (fText[fPos] == ' ' || fText[fPos] == '\t' || fText[fPos] == '\n' || fText[fPos] == '\r'))
      ++fPos;
  }

  bool Consume(const char* word)
  {
    const std::string w(word);
    if (fText.compare(fPos, w.size(), w) != 0) return false;
    fPos += w.size();
    return true;
  }

  JsonValue Value(int depth)
  {
    if (depth > 64) Fail("nesting deeper than 64");
    Blank();
    if (fPos >= fText.size()) Fail("unexpected end");
    const char c = fText[fPos];
    if (c == '{') return Object(depth);
    if (c == '[') return Array(depth);
    if (c == '"') return JsonValue::String(String());
    if (Consume("true")) return JsonValue::Bool(true);
    if (Consume("false")) return JsonValue::Bool(false);
    if (Consume("null")) return JsonValue();
    if (c == '-' || (c >= '0' && c <= '9')) return Number();
    Fail(std::string("unexpected character '") + c + "'");
  }

  JsonValue Object(int depth)
  {
    ++fPos;  // {
    auto object = JsonValue::Object();
    std::set<std::string> keys;
    Blank();
    if (fPos < fText.size() && fText[fPos] == '}')
    {
      ++fPos;
      return object;
    }
    for (;;)
    {
      Blank();
      if (fPos >= fText.size() || fText[fPos] != '"') Fail("expected a key");
      auto key = String();
      if (!keys.insert(key).second) Fail("repeated key \"" + key + "\"");
      Blank();
      if (fPos >= fText.size() || fText[fPos] != ':') Fail("expected ':'");
      ++fPos;
      object.Set(key, Value(depth + 1));
      Blank();
      if (fPos < fText.size() && fText[fPos] == ',')
      {
        ++fPos;
        continue;
      }
      if (fPos < fText.size() && fText[fPos] == '}')
      {
        ++fPos;
        return object;
      }
      Fail("expected ',' or '}'");
    }
  }

  JsonValue Array(int depth)
  {
    ++fPos;  // [
    auto array = JsonValue::Array();
    Blank();
    if (fPos < fText.size() && fText[fPos] == ']')
    {
      ++fPos;
      return array;
    }
    for (;;)
    {
      array.Append(Value(depth + 1));
      Blank();
      if (fPos < fText.size() && fText[fPos] == ',')
      {
        ++fPos;
        continue;
      }
      if (fPos < fText.size() && fText[fPos] == ']')
      {
        ++fPos;
        return array;
      }
      Fail("expected ',' or ']'");
    }
  }

  std::string String()
  {
    ++fPos;  // opening quote
    std::string text;
    for (;;)
    {
      if (fPos >= fText.size()) Fail("unterminated string");
      const char c = fText[fPos++];
      if (c == '"') return text;
      if (static_cast<unsigned char>(c) < 0x20) Fail("control character in a string");
      if (c != '\\')
      {
        text += c;
        continue;
      }
      if (fPos >= fText.size()) Fail("unterminated escape");
      const char e = fText[fPos++];
      switch (e)
      {
        case '"': text += '"'; break;
        case '\\': text += '\\'; break;
        case '/': text += '/'; break;
        case 'b': text += '\b'; break;
        case 'f': text += '\f'; break;
        case 'n': text += '\n'; break;
        case 'r': text += '\r'; break;
        case 't': text += '\t'; break;
        case 'u':
        {
          if (fPos + 4 > fText.size()) Fail("truncated \\u escape");
          const std::string hex = fText.substr(fPos, 4);
          if (hex.find_first_not_of("0123456789abcdefABCDEF") != std::string::npos) Fail("malformed \\u escape");
          const long code = std::strtol(hex.c_str(), nullptr, 16);
          if (code > 0x7f) Fail("\\u escape above 0x7f is not supported");
          text += static_cast<char>(code);
          fPos += 4;
          break;
        }
        default: Fail(std::string("unknown escape \\") + e);
      }
    }
  }

  JsonValue Number()
  {
    const std::size_t start = fPos;
    if (fText[fPos] == '-') ++fPos;
    const auto digits = [this] {
      const std::size_t first = fPos;
      while (fPos < fText.size() && fText[fPos] >= '0' && fText[fPos] <= '9') ++fPos;
      return fPos - first;
    };
    const std::size_t intStart = fPos;
    const std::size_t intDigits = digits();
    if (intDigits == 0) Fail("malformed number");
    if (intDigits > 1 && fText[intStart] == '0') Fail("leading zero in a number");
    bool real = false;
    if (fPos < fText.size() && fText[fPos] == '.')
    {
      ++fPos;
      real = true;
      if (digits() == 0) Fail("malformed number");
    }
    if (fPos < fText.size() && (fText[fPos] == 'e' || fText[fPos] == 'E'))
    {
      ++fPos;
      real = true;
      if (fPos < fText.size() && (fText[fPos] == '+' || fText[fPos] == '-')) ++fPos;
      if (digits() == 0) Fail("malformed number");
    }
    const std::string token = fText.substr(start, fPos - start);
    if (!real)
    {
      errno = 0;
      const long long value = std::strtoll(token.c_str(), nullptr, 10);
      if (errno != ERANGE) return JsonValue::Integer(value);
    }
    const double value = std::strtod(token.c_str(), nullptr);
    if (!std::isfinite(value)) Fail("number out of range");
    return JsonValue::Real(value);
  }

  std::string fText;
  std::size_t fPos = 0;
};

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

std::vector<DoseSpeciesRow> MakeDoseRows(const std::vector<double>& gridNs, const std::vector<double>& energyAtGridEv,
                                         const SpeciesTable& measured, const std::vector<double>& steadyCounts,
                                         const DoseNormalization& normalization)
{
  CheckNormalization(normalization);
  RequireRows(gridNs.size() == energyAtGridEv.size(), "one energy per grid time is needed");
  RequireRows(steadyCounts.empty() || steadyCounts.size() == measured.species.size(),
              "the steady counts need one count per species");
  std::vector<DoseSpeciesRow> rows;
  rows.reserve(gridNs.size() * measured.species.size());
  bool extrapolating = false;
  for (std::size_t k = 0; k < gridNs.size(); ++k)
  {
    const auto* counts = MeasuredAt(measured, gridNs[k]);
    if (counts == nullptr)
    {
      RequireRows(!steadyCounts.empty(), "the grid time " + FormatReal(gridNs[k]) +
                                             " ns was not measured and there is no steady state to extrapolate");
      extrapolating = true;
      counts = &steadyCounts;
    }
    else
      RequireRows(!extrapolating, "the grid time " + FormatReal(gridNs[k]) + " ns was measured after an extrapolated one");
    const double energy = energyAtGridEv[k];
    for (std::size_t s = 0; s < measured.species.size(); ++s)
    {
      DoseSpeciesRow row;
      row.timeNs = gridNs[k];
      row.species = measured.species[s];
      row.count = (*counts)[s];
      row.energyDepositEv = energy;
      row.doseGy = energy * kJoulePerEv / normalization.boxMassKg;
      row.gCumulativePer100eV = GPer100Ev(row.count, energy);
      row.gFinalPer100eV = GPer100Ev(row.count, normalization.finalEnergyEv);
      row.extrapolated = extrapolating;
      rows.push_back(std::move(row));
    }
  }
  return rows;
}

std::vector<AgeSpeciesRow> MakeAgeRows(const std::vector<AgePoint>& points, const std::vector<double>& energyAtPointEv,
                                       const SpeciesTable& measured, const DoseNormalization& normalization)
{
  CheckNormalization(normalization);
  RequireRows(points.size() == energyAtPointEv.size(), "one energy per age point is needed");
  std::vector<AgeSpeciesRow> rows;
  bool missing = false;
  for (std::size_t k = 0; k < points.size(); ++k)
  {
    const auto* counts = MeasuredAt(measured, points[k].timeNs);
    if (counts == nullptr)
    {
      missing = true;
      continue;
    }
    RequireRows(!missing, "the age point at " + FormatReal(points[k].timeNs) + " ns was measured after a missing one");
    const double energy = energyAtPointEv[k];
    for (std::size_t s = 0; s < measured.species.size(); ++s)
    {
      AgeSpeciesRow row;
      row.arrivalIndex = points[k].arrival;
      row.ageNs = points[k].ageNs;
      row.timeNs = points[k].timeNs;
      row.species = measured.species[s];
      row.count = (*counts)[s];
      row.energyDepositEv = energy;
      row.doseGy = energy * kJoulePerEv / normalization.boxMassKg;
      row.gCumulativePer100eV = GPer100Ev(row.count, energy);
      row.gFinalPer100eV = GPer100Ev(row.count, normalization.finalEnergyEv);
      rows.push_back(std::move(row));
    }
  }
  return rows;
}

void WriteDoseSpeciesCsv(std::ostream& out, const std::vector<DoseSpeciesRow>& rows)
{
  out << "timeNs,species,count,energyDepositEv,doseGy,gCumulativePer100eV,gFinalPer100eV,extrapolated\n";
  for (const auto& r : rows)
    out << CsvReal(r.timeNs) << ',' << CsvText(r.species) << ',' << CsvReal(r.count) << ','
        << CsvReal(r.energyDepositEv) << ',' << CsvReal(r.doseGy) << ',' << CsvReal(r.gCumulativePer100eV) << ','
        << CsvReal(r.gFinalPer100eV) << ',' << (r.extrapolated ? 1 : 0) << '\n';
  if (!out) throw std::runtime_error("Ledger: species stream failed");
}

void WriteAgeSpeciesCsv(std::ostream& out, const std::vector<AgeSpeciesRow>& rows)
{
  out << "arrivalIndex,ageNs,timeNs,species,count,energyDepositEv,doseGy,gCumulativePer100eV,gFinalPer100eV\n";
  for (const auto& r : rows)
    out << r.arrivalIndex << ',' << CsvReal(r.ageNs) << ',' << CsvReal(r.timeNs) << ',' << CsvText(r.species) << ','
        << CsvReal(r.count) << ',' << CsvReal(r.energyDepositEv) << ',' << CsvReal(r.doseGy) << ','
        << CsvReal(r.gCumulativePer100eV) << ',' << CsvReal(r.gFinalPer100eV) << '\n';
  if (!out) throw std::runtime_error("Ledger: species age stream failed");
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

const JsonValue* JsonValue::Find(const std::string& key) const
{
  for (const auto& member : members_)
    if (member.first == key) return &member.second;
  return nullptr;
}

double JsonValue::NumberValue() const
{
  if (type_ == Type::Integer) return static_cast<double>(integer_);
  if (type_ == Type::Real) return real_;
  throw std::runtime_error("JsonValue: not a number");
}

JsonValue ParseJson(std::istream& in)
{
  std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  if (in.bad()) throw std::runtime_error("JSON: read error");
  return JsonReader(std::move(text)).Document();
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

}  // namespace drs
