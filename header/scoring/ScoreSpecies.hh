/// \file ScoreSpecies.hh
/// \brief Accumulate Particle-based stage molecule counts and G-value sums.
/// Requires project settings/output and Geant4 scoring/chemistry services.
/// Worker-local scorers; merge and publish only at coordinated run boundaries.

// This example is provided by the Geant4-DNA collaboration
// Any report or published results obtained using the Geant4-DNA software
// shall cite the following Geant4-DNA collaboration publication:
// Med. Phys. 37 (2010) 4692-4708
// J. Comput. Phys. 274 (2014) 841-882
// The Geant4-DNA web site is available at http://geant4-dna.org
//
//
#ifndef CHEM4_ScoreSpecies_h
#define CHEM4_ScoreSpecies_h 1

#include "scoring/OutputTable.hh"

#include "G4THitsMap.hh"
#include "G4VPrimitiveScorer.hh"

#include <map>
#include <set>
#include <string>
#include <vector>

class G4MolecularConfiguration;

/** \file ScoreSpecies.hh*/

// Description:
//   Records radiolytic species produced after irradiation in a water volume.
//
// Created: 2015-10-27  by M. Karamitros,
// modified: 2016-03-16 by P. Piersimoni

/// Persistent scorer: counts molecules and sums per-event G values (molecules/100 eV).
/// Uses Geant4 event/counter services; callers coordinate master/worker lifecycles.
class ScoreSpecies : public G4VPrimitiveScorer
{
public:
  /// Construct scorer name at copy-number depth with the default record grid.
  ScoreSpecies(G4String name, G4int depth = 0);

  /// Destroy the scorer; event hit collections remain under Geant4 ownership.
  virtual ~ScoreSpecies();

  /// Add time in Geant4 internal units without range validation; duplicates collapse.
  /// Configure before events. The default grid ends just below 1 microsecond.
  inline void AddTimeToRecord(double time) { fTimeToRecord.insert(time); }

  /// Remove all record times; configure replacements before subsequent events.
  inline void ClearTimeToRecord() { fTimeToRecord.clear(); }

  /// Return the number of non-aborted events accumulated since the last clear/merge.
  inline int GetNumberOfRecordedEvents() const { return fNEvent; }

  /// Table "species" with SpeciesSchema's columns: one row per (time, species),
  /// times ascending (ns) then species sorted by name; nEvents is the number of
  /// accumulated events (zero-filled). Does not clear the scorer.
  OutputTable ToTable() const;

  /// Accumulated molecule count, count^2, sumG and sumG2 across accepted events.
  struct SpeciesInfo
  {
    /// Initialize all accumulated values to zero.
    SpeciesInfo()
    {
      fNumber = 0;
      fNumberSq = 0.;
      fG = 0.;
      fG2 = 0.;
    }
    /// Copy all accumulated values.
    SpeciesInfo(const SpeciesInfo &right)
    {
      fNumber = right.fNumber;
      fNumberSq = right.fNumberSq;
      fG = right.fG;
      fG2 = right.fG2;
    }
    /// Copy all values; self-assignment is a no-op.
    SpeciesInfo &operator=(const SpeciesInfo &right)
    {
      if (&right == this)
        return *this;
      fNumber = right.fNumber;
      fNumberSq = right.fNumberSq;
      fG = right.fG;
      fG2 = right.fG2;
      return *this;
    }
    int fNumber;
    double fNumberSq;
    double fG;
    double fG2;
  };

private:
  typedef const G4MolecularConfiguration Species;
  typedef std::map<Species *, SpeciesInfo> InnerSpeciesMap;
  typedef std::map<double, InnerSpeciesMap> SpeciesMap;
  SpeciesMap fSpeciesInfoPerTime;

  std::set<G4double> fTimeToRecord;

  int fNEvent;          // number of processed events
  double fEdep;         // total energy deposition

protected:
  /// Accumulate weighted energy from a live step; requires Initialize first.
  /// Returns false only for zero unweighted energy deposit.
  virtual G4bool ProcessHits(G4Step *, G4TouchableHistory *);

public:
  /// Register this event's hit map with a non-null HCE; called by Geant4.
  virtual void Initialize(G4HCofThisEvent *);
  /// Accumulate the current non-aborted event using the live molecule counter.
  /// Missing counter or negative molecule counts raise FatalException; HCE is unused.
  virtual void EndOfEvent(G4HCofThisEvent *);
  /// No visualization is produced.
  virtual void DrawAll();
  /// Print the current hit-map summary; requires a detector and initialized event map.
  virtual void PrintAll();
  /// Merge a worker ScoreSpecies and clear its aggregate counts/event state.
  /// Null, wrong-type and self arguments are ignored. Caller stops worker mutation;
  /// allocation failures propagate and can leave partial accumulation.
  virtual void AbsorbResultsFromWorkerScorer(G4VPrimitiveScorer *);
  /// On master/serial, clear aggregate counts and the event count (call after
  /// the Dump wrote ToTable()). Workers do nothing.
  virtual void OutputAndClear();

  /// Copy the accumulated map (internal time units, counts and G-value sums).
  /// Molecular-configuration pointers are borrowed; allocations may throw.
  SpeciesMap GetSpeciesInfo() { return fSpeciesInfoPerTime; }

private:
  void MergeSpeciesTotalsFrom(ScoreSpecies *right);

  G4int fHCID;
  G4THitsMap<G4double> *fEvtMap;
};
#endif
