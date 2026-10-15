# Ticket 02 Implementation

**Status:** ✅ Done

<!-- Set the Status line to exactly "**Status:** ✅ Done" only once the Verification Step passes. -->

## Commits

- feat: opt-in SBS rate-aware reaction acceptance (ticket 02) (SHA in git log)

## Local Test Result

```
build/ (RelWithDebInfo) sim: built clean (02-build-sim.log)
sim 02_beam_boscolo_rateaware.in --dir .../t02 : exit 0,
  "[RunAccumulatorMessenger] dumped and reset (prefix='EndOfRun_', subdir='')", no G4Exception,
  "[DnaChemistryList] ... reaction acceptance = rate-aware (RateAwareReactionModel)"
  t02/EndOfRun_Manifest.json: "sbs": {"rateAwareReactions": true, "maxTimeStep_ns": null}
sim beam_boscolo.in --dir .../t02-default : exit 0, dump line, no G4Exception,
  "... reaction acceptance = Geant4 Brownian bridge (G4DNASmoluchowskiReactionModel)"
  t02-default/EndOfRun_Manifest.json: "sbs": {"rateAwareReactions": false, "maxTimeStep_ns": null}
Reaction totals (10 keV, 2 events, 02-reaction-totals.txt), default -> rate-aware:
  OH+H2 47->0, OH+H2O2 26->0, H+OH- 20->0, H+H2O2 4->0, e_aq+H3O+ 195->96
ctest (build-ninja, Debug, all 11 targets built first): 100% tests passed, 0 failed out of 11
```

## Review Notes

- Deviation from the ticket text: Geant4 11.4.1 has no `G4DNAMolecularStepByStepModel(name, std::unique_ptr<G4VDNAReactionModel>)` constructor (only `(name, unique_ptr<G4VITTimeStepComputer>, unique_ptr<G4VITReactionProcess>)`). Used the default constructor plus `SetReactionModel(new RateAwareReactionModel())`; `Initialize()` keeps a preset model (G4DNAMolecularStepByStepModel.cc:69-74). The default path is still exactly `new G4DNAMolecularStepByStepModel()`.
- Deviation: Geant4's along-step continuation loop restarts at the axis where the in-radius loop broke and adds that axis twice (r2 overstated). RateAwareReactionModel resumes at k+1 (true r2). The in-radius branch is identical; `separationDistance` is discarded by the caller (`G4DNAMolecularReaction::TestReactibility`). Recorded in ADR 0006.
- `/chem/sbs/rateAwareReactions` uses `DeclareMethod` with a `G4bool` setter, not `DeclareProperty`: a bool property goes through `G4AnyType::FromString` (stream >> bool rejects "true"), while the method path converts with `StoB` (G4GenericMessenger.cc:245-268).
- e_aq + H3O+ also dropped by half with the switch on (one 2-event sample); its effective R is small relative to the long late-time steps, so this is the intended correction, but ticket 5's k-recovery check should look at it.
- README had a command section: updated too.

## Time Spent

~0.5 hours

## Blockers / Challenges

None

## Token Usage

- **Input:** 76
- **Output:** 7280
- **Cache read:** 3264085
- **Cache creation:** 108091
- **Total:** 3379532
