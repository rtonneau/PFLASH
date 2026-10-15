# Ticket 03 Implementation

**Status:** ✅ Done

## Commits

- e836636 feat: replace /chem/env/O2 with generic /chem/env/scavenger command (ticket 03)

## Local Test Result

```
ctest (build-ninja, Debug): 100% tests passed, 0 tests failed out of 8
Regression 03-t03 (no scavenger): sorted dump identical to 01-t01; EnergyDeposit.Txt and
PhysicsInteractions.csv identical to 01-baseline.

Command checks (<line> + /run/initialize):
c1 O2 21 mol        -> G4Exception InvalidScavenger: unknown unit 'mol' for O2 (use M, mM, uM, or % for O2)
c2 CO2 1 %          -> InvalidScavenger: unit % is only defined for O2 (...), not for CO2
c3 O2 -1 %          -> InvalidScavenger: negative concentration -1 for O2
c4 H3Op(B) 1 M      -> InvalidScavenger: H3Op(B) is set by the water/pH model (/chem/env/pH), ...
c5 Foo 1 mM         -> UnknownScavenger (DnaChemistryWorld::ConstructChemistryComponents) at init
c6 H2O2 1 mM        -> init OK; bulk composition: pH = 7.000000, H2O2 = 0.001000 M (4 components);
                       "scavenger H2O2 is inert: no bulk reaction of chemistry PureWater uses it as a partner"
c7 O2 21 % then 0 % -> bulk composition: pH = 7.000000 (3 components); no warning
c8 after init       -> "Illegal application state </chem/env/scavenger O2 21 %>", Batch is interrupted
c9 /chem/env/O2 21  -> "COMMAND NOT FOUND </chem/env/O2 21>", Batch is interrupted
```

## Review Notes

- `ScavengerMessenger` is a plain G4UImessenger with three 's' parameters, `SetToBeBroadcasted(false)`, constructed after the world's G4GenericMessenger.
- `GetConfiguration(name, false)` so our `UnknownScavenger` message is raised instead of Geant4's `CONF_NOT_CREATED`.
- Inert warning placed in `DnaChemistryList::ConstructReactionTable` (master-only), after the world composition is built.
- CLAUDE.md: also fixed the stale `BuildPureWaterAcidBase` / `AcidBaseList` names in the `PureWaterReactions.cc` Key Files line (missed in ticket 01).
- sim exit code 127 on fatal G4Exception is the normal abort path.

## Time Spent

~40 minutes

## Blockers / Challenges

None

## Token Usage

- **Input:** 32
- **Output:** 11388
- **Cache read:** 3909004
- **Cache creation:** 21582
- **Total:** 3942006
