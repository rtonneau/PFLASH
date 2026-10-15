/// \file PhysicsInteractionCounter.hh
/// \brief Portable string-frequency counter for physical interaction counts
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

class PhysicsInteractionCounter
{
 public:
  void Record(const G4String& label);

  /// Adds `other`'s counts into this counter; `other` is left unchanged.
  void Merge(const PhysicsInteractionCounter& other);

  void Clear();

  using Counts = std::map<G4String, G4long>;
  const Counts& GetCounts() const { return fCounts; }

  /// Writes one "<label>    count = N" line per label, sorted by label.
  void WriteAscii(std::ostream& out) const;

  /// Writes a "label,count" header followed by one row per label.
  void WriteCsv(std::ostream& out) const;

 private:
  Counts fCounts;
};

#endif  // PhysicsInteractionCounter_h
