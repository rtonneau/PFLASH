// Ledger: plain-text outputs of a history run (Geant4-free).
//
//   activations.csv  one row per injected source (WriteActivationsCsv)
//   species.csv      long layout: one row per (time, species) (WriteSpeciesCsv)
//   ledger.json      summary tree (JsonValue + WriteJson)
//
// Column names are camelCase with a unit suffix. Units: time ns, length nm,
// energy eV. Real numbers are written with 17 significant digits (round-trip).
// A non-finite value (e.g. an activation that was never observed) is written
// as an empty CSV field and as null in JSON.
//
// The tiny JsonValue tree is adapted from the dnachem-min DataNode/JsonWriter
// idea (ordered tree, Add on an existing key replaces it in place) but shares
// no code or header with it.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <string>
#include <utility>
#include <vector>

namespace ptirt
{

struct ActivationRecord
{
  std::size_t sourceIndex = 0;
  std::size_t packetIndex = 0;
  std::array<double, 3> offsetNm{};
  double scheduledTimeNs = 0;           // from the schedule
  double expectedActivationTimeNs = 0;  // scheduled time as the chemistry clock sees it
  double observedActivationTimeNs = 0;  // NaN if never observed
  long long firstTrackId = 0;
  long long lastTrackId = 0;
  std::size_t moleculeCount = 0;
  double energyDepositEv = 0;
  double escapedEnergyEv = 0;
};

void WriteActivationsCsv(std::ostream& out, const std::vector<ActivationRecord>& records);

struct SpeciesRow
{
  double timeNs = 0;
  std::string species;
  double count = 0;  // may be a mean over seeds
  std::string history;           // used only when includeHistoryColumns
  std::uint64_t chemistrySeed = 0;  // used only when includeHistoryColumns
};

// Header: timeNs,species,count  [+ ,history,chemistrySeed when includeHistoryColumns].
// Species and history text is CSV-quoted when it contains a comma, quote or newline.
void WriteSpeciesCsv(std::ostream& out, const std::vector<SpeciesRow>& rows,
                     bool includeHistoryColumns = false);

// Ordered JSON tree: null / bool / integer / real / string / array / object.
class JsonValue
{
 public:
  enum class Type { Null, Bool, Integer, Real, String, Array, Object };

  JsonValue() = default;  // null
  static JsonValue Bool(bool value);
  static JsonValue Integer(long long value);
  static JsonValue Real(double value);  // non-finite is written as null
  static JsonValue String(std::string value);
  static JsonValue Array();
  static JsonValue Object();

  Type GetType() const { return type_; }

  // Object: set key, replacing an existing entry in place. Throws if not an object.
  JsonValue& Set(const std::string& key, JsonValue value);
  // Array: append. Throws if not an array.
  JsonValue& Append(JsonValue value);

  const std::vector<std::pair<std::string, JsonValue>>& Members() const { return members_; }
  const std::vector<JsonValue>& Elements() const { return elements_; }
  bool BoolValue() const { return bool_; }
  long long IntegerValue() const { return integer_; }
  double RealValue() const { return real_; }
  const std::string& StringValue() const { return string_; }

 private:
  Type type_ = Type::Null;
  bool bool_ = false;
  long long integer_ = 0;
  double real_ = 0;
  std::string string_;
  std::vector<JsonValue> elements_;
  std::vector<std::pair<std::string, JsonValue>> members_;
};

// Containers whose members are all scalars are written on one line; others are
// indented by two spaces. Strings are escaped (quote, backslash, control chars).
void WriteJson(std::ostream& out, const JsonValue& value);

// Exposed for tests: JSON string literal including the quotes.
std::string JsonQuote(const std::string& text);

}  // namespace ptirt
