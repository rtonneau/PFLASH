/// \file PhysicsInteractionCounter.hh
/// \brief Count caller-supplied physical-interaction labels.
/// Standard library and Geant4 types; no kernel services.
/// Callers coordinate access to shared mutable counters and streams.
///
/// Self-contained, project-agnostic unit: no OutputDir, DnaLogger, or other
/// dnachem-min-specific includes -- only globals.hh (G4String) and the
/// standard library. Copy-paste portable to another Geant4-DNA project.
///
/// Counts occurrences of whatever label callers pass to Record() -- it has
/// no notion of "process" or "G4DNA"; callers decide what a label means and
/// whether to record it at all.

#ifndef PhysicsInteractionCounter_h
#define PhysicsInteractionCounter_h 1

#include "globals.hh"

#include <iosfwd>
#include <map>

/// Mutable count table; allocations may throw. Stream writers use stream flags
/// or enabled stream exceptions, without a separate success return.
class PhysicsInteractionCounter
{
 public:
  /// Increment label by one; labels are accepted without interpretation.
  void Record(const G4String& label);

  /// Add other's counts; a distinct source is unchanged. Self-merge doubles counts.
  void Merge(const PhysicsInteractionCounter& other);

  /// Remove all recorded counts; leave configuration unchanged.
  void Clear();

  using Counts = std::map<G4String, G4long>;
  /// Borrow the stored counts; callers must coordinate concurrent mutation.
  const Counts& GetCounts() const { return fCounts; }

  /// Writes one "<label>    count = N" line per label, sorted by label.
  void WriteAscii(std::ostream& out) const;

  /// Write a "label,count" header and sorted rows; labels are not CSV-escaped.
  void WriteCsv(std::ostream& out) const;

 private:
  Counts fCounts;
};

#endif  // PhysicsInteractionCounter_h
