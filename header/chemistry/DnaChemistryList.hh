/// \file DnaChemistryList.hh
/// Constructs the selected Chemistry and its Geant4 processes.
/// Dependencies: Geant4 and project services; not standard-library-only.
/// Thread safety: Master builds shared reactions; process and scheduler setup follows Geant4 thread callbacks.
///
/// Project chemical stage, replacing the macro-driven `/chem/species` +
/// `/chem/reaction/add` blocks. Inherits BOTH `G4VUserChemistryList`
/// (molecules / reactions / time-step model) and `G4VPhysicsConstructor`
/// (driven directly by PhysicsList, UHDR-style).
///
/// Reaction content: the Chemistry chosen with `/chem/select <name>` before
/// `/run/initialize` (default `PureWater`; see ChemistryRegistry). It supplies
/// the ordinary reaction table and the bulk-reaction list (reactions against
/// bulk species: the pH-driven acid-base buffer against H3Op(B)/OHm(B), UHDR:
/// ChemPureWaterBuilder), registered as per-molecule `G4DNAScavengerProcess`
/// for the particle-based stage and added to the reaction table for the
/// mesoscopic stage.
/// `PureWater` is the portable pure-water + O2-derived network
/// (PureWaterReactions.cc) with the full acid-base network; another Chemistry
/// may omit that buffer
/// (docs/adr/0002-named-chemistries.md).
/// Exogenous scavengers (e.g. dissolved O2, `/chem/env/scavenger` on
/// DnaChemistryWorld) react through the Chemistry's bulk reactions
/// (docs/adr/0004-scavenger-reactions-per-chemistry.md).
///
/// Time-step model (/process/chem/TimeStepModel, default IRT_syn set in
/// PhysicsList): IRT_syn, whose particle-based stage hands over to the
/// mesoscopic stage in TimeStepAction unless /chem/meso/enable false, or SBS
/// (step-by-step, no mesoscopic stage). IRT and any other value are fatal.

#ifndef DnaChemistryList_h
#define DnaChemistryList_h 1

#include "chemistry/ChemistryTypes.hh"

#include "G4VPhysicsConstructor.hh"
#include "G4VUserChemistryList.hh"
#include "globals.hh"

#include <memory>
#include <set>

namespace ChemistryRegistry
{
struct Chemistry;
}
class ChemistrySelectMessenger;
class MesoMessenger;
class G4DNABoundingBox;
class G4DNAMolecularReactionData;
class G4DNAMolecularReactionTable;
class G4GenericMessenger;
class DnaChemistryWorld;

/// Geant4 lifecycle adapter for the selected Chemistry; invalid configuration is fatal.
class DnaChemistryList : public G4VUserChemistryList, public G4VPhysicsConstructor
{
public:
  /// Registers built-ins and UI controls with the Geant4 managers on the master.
  /// Registration errors raise FatalException; not a standalone portable object.
  DnaChemistryList();
  /// Releases owned messengers after UI dispatch has stopped.
  ~DnaChemistryList() override;

  // --- G4VPhysicsConstructor ---
  /// Physics-constructor hook forwarding to ConstructMolecule; same preconditions.
  void ConstructParticle() override { ConstructMolecule(); }
  /// Registers processes, initializes Chemistry and installs thread-local scavenger material.
  /// Requires this project's detector/world; invalid model or missing world is fatal.
  void ConstructProcess() override;

  // --- G4VUserChemistryList ---
  /// Builds shared molecules and the selected Chemistry's Extra molecules before processes.
  /// Runs in Geant4 initialization; missing selected Chemistry raises FatalException.
  void ConstructMolecule() override;
  /// Registers option1 water dissociation after molecule construction; Geant4 handles errors.
  void ConstructDissociationChannels() override;
  /// Populates non-null reactionTable on the master after molecules/world exist.
  /// Missing species/world or conflicting bulk pairs raise FatalException; rates use Geant4 units.
  void ConstructReactionTable(G4DNAMolecularReactionTable* reactionTable) override;
  /// Registers the selected time-step model per thread; reactionTable is unused.
  /// Unsupported models raise FatalException; called by Geant4 initialization.
  void ConstructTimeStepModel(G4DNAMolecularReactionTable* reactionTable) override;

  /// Resolves any /chem/reaction/timeBinsFixed or /chem/reaction/timeBinsList
  /// macro command into ReactionCounter::ConfigureBinEdges() -- a no-op if
  /// neither was issued (ReactionCounter keeps its built-in default table).
  /// Must be called after the chemistry scheduler's end time is final (i.e.
  /// from RunAction::BeginOfRunAction, not any earlier), since the "fixed"
  /// mode expands its step into edges up to that end time. Raises a
  /// FatalException if both commands were issued, or if timeBinsList's value
  /// is malformed (parsing is deferred to here, not macro-issue time, since
  /// timeBinsList is a plain string property -- see fReactionTimeBinsList).
  /// Fixed width must be finite and positive (currently not validated); times use
  /// Geant4 internal units. No bool/error-string result is provided.
  void ApplyReactionTimeBinning() const;

  /// Reaction-table entries this driver added only for the mesoscopic stage (bulk
  /// reactions whose pair was not already a tracked entry). Filled on the master
  /// during ConstructReactionTable; read-only afterwards. Pointers are borrowed
  /// from the reaction table.
  const std::set<const G4DNAMolecularReactionData*>& MesoOnlyReactions() const
  {
    return fMesoOnlyReactions;
  }

private:
  /// Fatal unless the time-step model is IRT_syn or SBS, and when
  /// /chem/meso/enable true was given explicitly with SBS; otherwise prints
  /// the model and whether the mesoscopic stage runs. Master, at
  /// /run/initialize (ConstructProcess).
  void CheckTimeStepModel() const;

  /// /chem/reaction/timeBinsFixed <width> <unit> setter.
  void SetReactionTimeBinsFixed(G4double width);

  /// The Chemistry chosen with /chem/select (default PureWater); fatal if
  /// none is available.
  const ChemistryRegistry::Chemistry* SelectedChemistry(const G4String& caller) const;

  /// The project chemistry world, via the run manager's detector.
  const DnaChemistryWorld* ChemistryWorld(const G4String& caller) const;

  /// Registers one G4DNAScavengerProcess per entry of `list`: the bulk
  /// reactions, e.g. the pH-driven acid-base buffer equilibria against the
  /// bulk H3Op(B) / OHm(B) / H2O pseudo-species (UHDR:
  /// ChemPureWaterBuilder::WaterScavengerReaction).
  /// The values come from the Chemistry (PureWater: always the full network);
  /// an empty list registers nothing.
  void RegisterBulkReactionProcesses(const G4DNABoundingBox& boundary,
                                     const ChemistryTypes::BulkReactionList& list) const;

  /// Adds every entry of `list` to the reaction table too, one
  /// G4DNAMolecularReactionData per bulk reaction with the configurations,
  /// products and reaction type RegisterBulkReactionProcesses uses (UHDR:
  /// ChemPureWaterBuilder::WaterScavengerReaction): the mesoscopic stage
  /// (G4DNAGillespieDirectMethod) only reads the reaction table, and takes a
  /// bulk partner's count from G4DNAScavengerMaterial. Pure bulk pseudo-species
  /// have no tracks to pair in the particle-based stage. A pair already in
  /// the table (bulk O2 = the tracked "O2" configuration) shares
  /// that entry, which must then have the same rate and products (fatal
  /// otherwise).
  void AddBulkReactionsToTable(G4DNAMolecularReactionTable* reactionTable,
                               const ChemistryTypes::BulkReactionList& list);

  /// Entries AddBulkReactionsToTable created (see MesoOnlyReactions).
  std::set<const G4DNAMolecularReactionData*> fMesoOnlyReactions;

  /// Warns about every type-1 (partially diffusion-controlled) entry whose
  /// derived activation rate is <= 0. The singular equal-rate case is not detected.
  void WarnOnNegativeActivationRates(G4DNAMolecularReactionTable* reactionTable) const;

  /// Exposes /chem/select <name> and /chem/list.
  std::unique_ptr<ChemistrySelectMessenger> fSelectMessenger;
  std::unique_ptr<MesoMessenger> fMesoMessenger;

  /// Exposes /chem/reaction/dump <filename>.
  std::unique_ptr<G4GenericMessenger> fMessenger;

  /// Target file for ConstructProcess() to dump the reaction table to;
  /// empty (default) disables the dump.
  G4String fReactionDumpFile;

  /// Target file for the JSON catalogue export (/chem/reaction/exportJson);
  /// empty (default) disables it.
  G4String fCatalogExportFile;

  /// True once /chem/reaction/timeBinsFixed has been issued.
  G4bool fReactionBinWidthSet = false;

  /// Set by timeBinsFixed in Geant4 time units (ns); used when fReactionBinWidthSet.
  G4double fReactionBinWidth = 0.;

  /// Raw /chem/reaction/timeBinsList value, e.g. "1 10 100 picosecond";
  /// empty (default) means the command wasn't issued. A DeclareProperty
  /// (not DeclareMethod): G4GenericMessenger's method dispatch re-tokenizes
  /// a combined multi-token command value by the bound function's argument
  /// count, truncating a "<e1> ... <eN> <unit>" string to just its first
  /// token; DeclareProperty's G4String::FromString() assigns it intact.
  /// Parsed lazily by ApplyReactionTimeBinning().
  G4String fReactionTimeBinsList;
};

#endif // DnaChemistryList_h
