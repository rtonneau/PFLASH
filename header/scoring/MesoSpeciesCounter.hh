/// \file MesoSpeciesCounter.hh
/// \brief Portable (time, species) -> molecule-count table for the
/// mesoscopic stage's record times
///
/// Self-contained, project-agnostic unit: standard library only, no Geant4
/// or other dnachem-min includes. Copy-paste portable to another Geant4-DNA
/// project (adjust the include path prefix on copy). Same shape as
/// PhysicsInteractionCounter.
///
/// Times are in ns (the outputs say so). Counts are sums over events;
/// WriteAscii divides by the event count it is given.

#ifndef MesoSpeciesCounter_h
#define MesoSpeciesCounter_h 1

#include <iosfwd>
#include <map>
#include <string>

class MesoSpeciesCounter
{
 public:
  /// Adds `count` molecules of `species` at `time` (sums with earlier Adds).
  void Add(double time, const std::string& species, long count);

  /// Adds `other`'s counts into this counter; `other` is left unchanged.
  void Merge(const MesoSpeciesCounter& other);

  void Clear();

  bool Empty() const;

  using Counts = std::map<double, std::map<std::string, long>>;
  const Counts& GetCounts() const { return fCounts; }

  /// A "Time is in ns; mean count per event over N events" line, then per
  /// time (ascending): a line with the time, then one
  /// "<species> <mean count per event>" line per species, sorted by name.
  /// The means are 0 when events <= 0.
  void WriteAscii(std::ostream& out, long events) const;

  /// Writes a "time_ns,species,count" header, then one row per
  /// (time, species), times ascending then species sorted, holding the
  /// summed count.
  void WriteCsv(std::ostream& out) const;

 private:
  Counts fCounts;
};

#endif  // MesoSpeciesCounter_h
