/// \file MesoSpeciesCounter.hh
/// \brief Accumulate molecule counts at Mesoscopic stage record times.
/// Standard library only.
/// Callers coordinate access to shared mutable counters and streams.
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

/// Mutable count table; allocations may throw. Stream writers use stream flags
/// or enabled stream exceptions, without a separate success return.
class MesoSpeciesCounter
{
 public:
  /// Add count molecules of species at time_ns (ns); values are not validated.
  void Add(double time_ns, const std::string& species, long count);

  /// Add other's counts; a distinct source is unchanged. Self-merge doubles counts.
  void Merge(const MesoSpeciesCounter& other);

  /// Remove all recorded counts; leave configuration unchanged.
  void Clear();

  /// True when no time/species entries have been recorded.
  bool Empty() const;

  using Counts = std::map<double, std::map<std::string, long>>;
  /// Borrow the stored counts; callers must coordinate concurrent mutation.
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
