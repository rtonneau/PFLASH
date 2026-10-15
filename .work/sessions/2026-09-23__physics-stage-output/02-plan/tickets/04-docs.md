# Ticket 04: docs

**Acceptance Criteria:**
- [ ] `CLAUDE.md`'s Key Files section has new bullets for `src/PhysicsInteractionCounter.cc` (portable string-frequency counter, copy-paste convention noted, same as `PureWaterReactions.cc`) and `src/SteppingAction.cc` (per-step G4DNA process-name filter into the counter).
- [ ] The existing `RunAction.cc`/`Run.cc` Key Files bullets (or a nearby clause) mention they also write `EnergyDeposit.Txt` and `PhysicsInteractions.Txt`/`.csv`, merging `PhysicsInteractionCounter` the same way as `ReactionCounter`.
- [ ] The "Build and Run" output-files paragraph documents `EnergyDeposit.Txt` and `PhysicsInteractions.Txt`/`.csv` alongside the existing `Species.Txt`/`Reactions.Txt` description, noting: totals only (no time binning), and only discrete G4DNA physics processes are counted (`Transportation` excluded).

**Files to Touch:**
- `CLAUDE.md`

**Verification Step:**

Run:
```powershell
git diff CLAUDE.md
```

Expected:
The diff shows the new Key Files bullets and the extended output-files paragraph, matching the actual behavior implemented in Tickets 01-03 (file names, filter rule, totals-only framing) — read it back against `src/RunAction.cc`, `src/SteppingAction.cc`, and `src/PhysicsInteractionCounter.cc` to confirm no drift.

**Notes:**

Docs-only ticket; no build or smoke-run required beyond a proofreading pass against the code produced by Tickets 01-03. Suggested Key Files bullets:

```markdown
- `src/PhysicsInteractionCounter.cc`: portable, project-agnostic string-frequency counter (`Record`/`Merge`/`Clear`/`WriteAscii`/`WriteCsv`, stream-only I/O, no dnachem-min-specific includes) — copy-paste portable to another Geant4-DNA project, same convention as `PureWaterReactions.cc`.
- `src/SteppingAction.cc`: per-step physical-interaction counting — records `G4Step::GetPostStepPoint()->GetProcessDefinedStep()->GetProcessName()` into a `PhysicsInteractionCounter` whenever the name contains `"G4DNA"` (discrete G4DNA physics processes only, excludes `Transportation`).
```

Suggested addition to the "Build and Run" output-files paragraph:

```markdown
Each run also writes `EnergyDeposit.Txt` (total energy deposited in the simulation
volume, human-readable) and `PhysicsInteractions.Txt`/`PhysicsInteractions.csv`
(per-process physical-interaction firing counts — totals only, no time binning;
only discrete G4DNA physics processes are counted, e.g. `e-_G4DNAIonisation`,
`e-_G4DNAExcitation`, `e-_G4DNAElastic`, `e-_G4DNAVibExcitation`,
`e-_G4DNAAttachment` — `Transportation` and other bookkeeping steps are excluded).
```
