// Ledger: plain-text outputs of a history run (Geant4-free).
//
//   activations.csv  one row per injected source (WriteActivationsCsv)
//   species.csv      long layout: one row per (time, species) (WriteSpeciesCsv;
//                    WriteDoseSpeciesCsv in doseCutoff mode)
//   species_age.csv  doseCutoff mode: one row per (arrival, age, species)
//                    (WriteAgeSpeciesCsv)
//   ledger.json      summary tree (JsonValue + WriteJson); ParseJson reads one back
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
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "TimeGrid.hh"

namespace drs
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

// ---- doseCutoff mode: species counts with the deposited energy ----------------
//
// N(t) is the count of a species at the record time t, E(t) the energy deposited
// in the box by the sources already in the history at t (TimeGrid.hh,
// CumulativeEnergy), D(t) = E(t) / box mass, and E_final the energy of the
// whole schedule (every source up to the cutoff: the dose at beam-off). Two
// G-values, molecules per 100 eV:
//   gCumulativePer100eV = 100 N(t) / E(t)      (empty when E(t) = 0)
//   gFinalPer100eV      = 100 N(t) / E_final
// After a steady-state stop the counts are no longer measured: the remaining
// common-grid times get the steady count N_ss of each species (flagged
// extrapolated = 1) with their own E(t); the remaining age-grid points are
// omitted.

inline constexpr double kJoulePerEv = 1.602176634e-19;

// Measured counts: one count per species (fixed order) per record time.
struct SpeciesTable
{
  std::vector<std::string> species;
  std::map<double, std::vector<double>> countsAt;
};

struct DoseNormalization
{
  double finalEnergyEv = 0;  // > 0
  double boxMassKg = 0;      // > 0
};

struct DoseSpeciesRow
{
  double timeNs = 0;
  std::string species;
  double count = 0;
  double energyDepositEv = 0;      // E(t)
  double doseGy = 0;               // E(t) / box mass
  double gCumulativePer100eV = 0;  // NaN when E(t) = 0
  double gFinalPer100eV = 0;
  bool extrapolated = false;
};

struct AgeSpeciesRow
{
  std::size_t arrivalIndex = 0;
  double ageNs = 0;
  double timeNs = 0;
  std::string species;
  double count = 0;
  double energyDepositEv = 0;
  double doseGy = 0;
  double gCumulativePer100eV = 0;
  double gFinalPer100eV = 0;
};

// Rows (time order, then species order) for every time of gridNs, whose E(t)
// is energyAtGridEv (same length). A time present in measured.countsAt is
// measured; a time absent from it takes steadyCounts (one per species) and is
// flagged extrapolated. Throws std::invalid_argument when a time is absent and
// steadyCounts is empty, when a measured time follows an extrapolated one, on
// a size mismatch, or on a non-positive normalization.
std::vector<DoseSpeciesRow> MakeDoseRows(const std::vector<double>& gridNs, const std::vector<double>& energyAtGridEv,
                                         const SpeciesTable& measured, const std::vector<double>& steadyCounts,
                                         const DoseNormalization& normalization);

// Rows for the age points (energyAtPointEv: E(t) of each point) that were
// measured; the unmeasured ones (after a steady-state stop) are omitted and
// must all come after the measured ones (else std::invalid_argument).
std::vector<AgeSpeciesRow> MakeAgeRows(const std::vector<AgePoint>& points, const std::vector<double>& energyAtPointEv,
                                       const SpeciesTable& measured, const DoseNormalization& normalization);

// Header: timeNs,species,count,energyDepositEv,doseGy,gCumulativePer100eV,gFinalPer100eV,extrapolated
void WriteDoseSpeciesCsv(std::ostream& out, const std::vector<DoseSpeciesRow>& rows);
// Header: arrivalIndex,ageNs,timeNs,species,count,energyDepositEv,doseGy,gCumulativePer100eV,gFinalPer100eV
void WriteAgeSpeciesCsv(std::ostream& out, const std::vector<AgeSpeciesRow>& rows);

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
  // Object member, or nullptr (also when this is not an object).
  const JsonValue* Find(const std::string& key) const;
  // Integer or real as a double; throws std::runtime_error for any other type.
  double NumberValue() const;
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

// Strict reader for the JSON files of this project (capture.json): one value,
// then only whitespace. A number with a fraction or an exponent is a real,
// any other an integer (a real when it overflows long long). Strings may hold
// the escapes JsonQuote writes, and \/; a \u escape above 0x7f is rejected.
// A repeated object key is rejected. Throws std::runtime_error
// ("JSON: ... at offset N") on any violation.
JsonValue ParseJson(std::istream& in);

}  // namespace drs
