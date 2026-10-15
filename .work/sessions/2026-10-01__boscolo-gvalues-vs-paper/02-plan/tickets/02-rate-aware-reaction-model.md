# Ticket 02: rate-aware-reaction-model

**Model:** opus

**Acceptance Criteria:**
- [ ] `RateAwareReactionModel` (`header/chemistry/RateAwareReactionModel.hh`, `src/chemistry/RateAwareReactionModel.cc`) derives from `G4DNASmoluchowskiReactionModel` and overrides `FindReaction(const G4Track&, const G4Track&, G4double reactionRadius, G4double& separationDistance, G4bool alongStepReaction)`. The in-radius branch and the separation output match Geant4's (`G4DNASmoluchowskiReactionModel.cc:63-133`). The along-step branch draws `G4UniformRand() <= EncounterProbability(preStepSeparation, postStepSeparation, reactionRadius, D_A + D_B, trackB.GetStep()->GetDeltaTime())`.
- [ ] `/chem/sbs/rateAwareReactions <bool>` (PreInit, default false) is declared on `DnaChemistryList`'s messenger setup in a new `/chem/sbs/` directory. When it is true, `DnaChemistryList::ConstructTimeStepModel` registers `new G4DNAMolecularStepByStepModel("DNAMolecularStepByStepModel", std::make_unique<RateAwareReactionModel>())`. When it is false, the line stays exactly `new G4DNAMolecularStepByStepModel()`. A `DnaLogger` Info line states which acceptance is in use.
- [ ] `DnaChemistryList` exposes `G4bool IsRateAwareReactions() const`.
- [ ] `Manifest.json` gains a top-level object `"sbs": {"rateAwareReactions": <bool>, "maxTimeStep_ns": null}` (`maxTimeStep_ns` is filled by ticket 3), read through `PhysicsList::GetChemistryList()` the way `RunAction` reaches it.
- [ ] `docs/adr/0006-opt-in-sbs-rate-aware-acceptance.md` (ADR format of the existing ADRs) records: the artifact (with this session's reaction-count evidence), the exact formula, opt-in vs global (PureWater baselines unchanged), and the alternatives that were rejected (step cap alone, TRAX-style no bridge, contact radius + per-encounter probability).
- [ ] CLAUDE.md (Key Files: new file entry; Macro section: the new command), `.claude/skills/sim-output/SKILL.md` (Manifest key list) and README's command list, if it has one, are updated.
- [ ] `sim beam_boscolo.in` runs with `/chem/sbs/rateAwareReactions true` added in a scratch copy: exit 0, the dump line is present, and Manifest shows `"rateAwareReactions": true`. The unmodified `beam_boscolo.in` shows `false`.
- [ ] ctest is still all green.

**Files to Touch:**
- `header/chemistry/RateAwareReactionModel.hh`
- `src/chemistry/RateAwareReactionModel.cc`
- `header/chemistry/DnaChemistryList.hh`
- `src/chemistry/DnaChemistryList.cc`
- `src/scoring/RunManifest.cc`
- `docs/adr/0006-opt-in-sbs-rate-aware-acceptance.md`
- `CLAUDE.md`
- `.claude/skills/sim-output/SKILL.md`

**Verification Step:**

Run:
```bash
cmake --build build --target sim && cd build && ./sim.exe <scratch copy of beam_boscolo.in with /chem/sbs/rateAwareReactions true> --dir ../.scratch/tests/2026-10-01__boscolo-gvalues-vs-paper/t02 && grep -A2 '"sbs"' ../.scratch/tests/2026-10-01__boscolo-gvalues-vs-paper/t02/Manifest.json
```

Expected:
Exit 0, `[RunAccumulatorMessenger] dumped and reset` in the output, `"rateAwareReactions": true`.

**Notes:**

The `G4GenericMessenger` pitfall from this project's memory: `DeclareMethod` truncates multi-token string args. A bool via `DeclareProperty` or `DeclareMethod` with a `G4bool` parameter is fine. Create the `/chem/sbs/` directory with its own `G4GenericMessenger`, not on `/chem/` (`G4DNAChemistryManager` owns `/chem/`). `GetMolecule(const G4Track&)` is the Geant4 free function from `G4Molecule.hh`. The model is constructed per thread inside `ConstructTimeStepModel`; read the flag from the shared `DnaChemistryList` member (set in PreInit before workers exist).
