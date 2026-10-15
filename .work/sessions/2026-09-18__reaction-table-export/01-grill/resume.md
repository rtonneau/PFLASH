# Session: reaction-table-export

**Date:** 2026-09-18T09:55:55.377Z
**Status:** Grill phase complete

## Problem Statement

There is no way to see, in a durable file, what chemical reactions the simulation actually
registered for a run. The two networks the chemistry stage builds — the shared bimolecular
pure-water + O2 network, and the per-molecule acid-base/bulk scavenger network — are only
ever readable by stepping through code or trusting the physics silently did the right thing.
Add a way to dump the reaction table to a file so the reaction set can be checked (e.g. against
the source rates in the code, or across runs after a change) without re-reading C++.

## Context & Constraints

- **Current behavior:** `DnaChemistryList::ConstructReactionTable` builds the shared
  `G4DNAMolecularReactionTable` via `PureWaterReactions::BuildPureWaterReactions` (pure water +
  O2-derived second-order network). Separately, `DnaChemistryList::ConstructProcess` calls
  `RegisterAcidBaseScavengerProcesses`, which builds one `G4DNAScavengerProcess` per molecule
  with its own private reaction map (`H3Op(B)`/`OHm(B)` bulk exchanges) — this network is never
  added to the shared `G4DNAMolecularReactionTable`. Nothing currently writes either network
  to a file; `DnaLogger::Print` only logs short status lines ("reaction table constructed …").
- **Pain point:** No artifact exists to check "what reactions actually got registered" — not
  after code changes, not as a sanity check before trusting a run's chemistry.
- **Dependencies:** Must not duplicate reaction data into project-side data structures — the
  data should come from Geant4's own live objects at dump time, not a shadow copy captured
  during construction. `G4DNAMolecularReactionTable` already exposes this cleanly
  (`GetVectorOfReactionData()`, a real singleton utility). `G4DNAScavengerProcess::fConfMap`
  (the per-molecule acid-base reaction map) is `protected` with no accessor — reading it live
  requires a thin subclass that exposes it, rather than recording the data separately in
  `DnaChemistryList.cc` while it's being built.
- **Tech stack:** Geant4-DNA (`G4DNAMolecularReactionTable`, `G4DNAMolecularReactionData`,
  `G4DNAScavengerProcess`, `G4ProcessTable`, `G4MoleculeTable`), project's `DnaLogger` /
  `G4UImessenger` macro-command conventions (see `DnaLoggerMessenger.cc`,
  `DnaChemistryWorld`'s `/chem/env/pH` / `/chem/env/O2` PreInit commands).

## Success Metrics

- Adding `/chem/reaction/dump <filename>` to a macro (e.g. `beam.in`) and running produces a
  plain-text file listing every reaction in both the bimolecular network and the acid-base
  network, each line legible as `A + B -> C + D    k = <rate> M^-1 s^-1`.
- Every reaction line's rate and products are read from the live Geant4 objects
  (`G4DNAMolecularReactionTable`, and the acid-base process instances via the process table) at
  dump time — not from a local record kept in `DnaChemistryList.cc`/`DnaChemistryWorld.cc`.
- Without the macro command, behavior is unchanged (no file written, no overhead beyond the
  now-always-populated acid-base map, which was already being built regardless).

## Architecture & Approach

Two independent read paths feeding one writer, triggered by a new PreInit-only macro command.

**Bimolecular network (pure water + O2):** read directly from the existing Geant4 singleton —
`G4DNAMolecularReactionTable::Instance()->GetVectorOfReactionData()` — no project-side changes
needed to reach this data; it was already fully assembled by `PureWaterReactions`.

**Acid-base/scavenger network:** `G4DNAScavengerProcess::fConfMap` (its own private reaction
map, one process instance per participating molecule) is `protected`, with no public getter.
Add a minimal subclass:

```cpp
// exposes fConfMap for read access; no new data, no new behavior
class ScavengerReactionAccess : public G4DNAScavengerProcess {
 public:
  using G4DNAScavengerProcess::G4DNAScavengerProcess;
  const std::map<MolType, std::map<MolType, Data*>>& GetReactionMap() const { return fConfMap; }
};
```

`DnaChemistryList::RegisterAcidBaseScavengerProcesses`'s `build` lambda instantiates
`ScavengerReactionAccess` instead of `G4DNAScavengerProcess` directly — identical construction,
identical registration, just exposes the same live state for later reading. No local `Rx`
shadow-list is kept.

At dump time, the writer iterates `G4MoleculeTable::Instance()`'s molecule definitions, looks
up `G4ProcessTable::GetProcessTable()->FindProcess("G4DNAScavengerProcess", <name>)` per
molecule (same lookup pattern `ConstructProcess` already uses for the vib-excitation process),
`dynamic_cast`s to `ScavengerReactionAccess`, and walks `GetReactionMap()` to emit lines.

**Trigger:** new macro command `/chem/reaction/dump <filename>`, PreInit-only (same state
restriction as `/chem/env/pH`/`/chem/env/O2`), via a small dedicated messenger following
`DnaLoggerMessenger`'s shape. The command only stores the target filename (empty = disabled);
the actual dump happens once both networks are fully constructed, at the end of
`DnaChemistryList::ConstructProcess` (after `RegisterAcidBaseScavengerProcesses` and the
`G4DNAChemistryManager::Instance()->Initialize()` call that populates the bimolecular table).

**Output format:** plain text, human-readable, two labeled sections (bimolecular / acid-base),
one line per reaction: `A + B -> C + D    k = <rate> M^-1 s^-1`, species names read via
`GetDefinition()->GetName()`.

**New files:** `src/ReactionTableDump.cc`/`.hh` (writer functions + the
`ScavengerReactionAccess` subclass) — a portable-utility-style file like `PureWaterReactions.cc`,
plus a small messenger (or an addition to an existing one, TBD at implementation time) for the
new macro command.

## Assumptions & Trade-offs

- Bimolecular and acid-base reactions are dumped as two separate labeled sections rather than
  interleaved — simpler to implement and read, and matches how the two networks are genuinely
  distinct objects in Geant4.
- The dump only fires if `/chem/reaction/dump <filename>` is present in the macro; normal runs
  without that command see zero behavior change (aside from `ScavengerReactionAccess` replacing
  `G4DNAScavengerProcess` as the concrete type registered — functionally identical, negligible
  overhead).
- Not doing: no CSV/JSON output variant (plain text only, per explicit choice), no attempt to
  compute/emit derived quantities (reaction radius, diffusion constants) beyond rate + products,
  no dump of the water-dissociation channels (`ConstructDissociationChannels`) — scope is
  reactions only, as originally asked.
- This is bounded work: existing flow (`DnaChemistryList::ConstructReactionTable`/
  `ConstructProcess`) is being extended, not restructured; no new subsystem or architectural
  shift.

## Open Questions

None outstanding — user confirmed the "pulled directly from Geant4 utilities" constraint is
satisfied by reading `G4DNAMolecularReactionTable` and the live `ScavengerReactionAccess`
process objects at dump time, rather than any shadow copy in `DnaChemistryList.cc`/
`DnaChemistryWorld.cc`. Exact placement of the new messenger (own class vs. extending an
existing one) is left as an implementation-time judgment call, not a design gap.

## Notes

- Prior art for macro command conventions: `DnaLoggerMessenger.cc` (simple `G4UImessenger`
  wrapping one `G4UIcmd*`), and `DnaChemistryWorld`'s `/chem/env/pH`/`/chem/env/O2` PreInit-only
  commands.
- `G4DNAMolecularReactionTable::PrintTable()` exists in Geant4 itself but was deliberately not
  reused — it requires a `G4VDNAReactionModel` to compute interaction radii (jump step = 3 ps)
  and prints only to `G4cout`, not a file; the simpler `GetVectorOfReactionData()` +
  `GetObservedReactionRateConstant()`/`GetProducts()` accessors give exactly the reactant/
  rate/product triple needed without that extra machinery.

## Token Usage

- **Input:** 76
- **Output:** 24538
- **Cache read:** 2957860
- **Cache creation:** 259050
- **Total:** 3241524
