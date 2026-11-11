# Notes for a possible Geant4 upstream report

**Status: prepared, not submitted.** Nothing here has been sent to the Geant4
collaboration, and no issue number exists. These are observations from Geant4 11.4.3
(stock) in this example, with the evidence that supports each. They are candidates; the
reader should confirm each against the current Geant4 source before reporting.
Evidence root: `.scratch/tests/2026-11-10__poisson-track-irt-stock-build/` (local).

## 1. Contact gap: pairs within the reaction radius are trialled only at the scheduler start time

`G4DNAIndependentReactionTimeStepper.cc` (lines about 167 to 173) trials a pair that is
already within its effective reaction radius only while `GetGlobalTime() == GetStartTime()`.
A molecule that enters later, at a delayed track time, is never trialled against
molecules already there (a **New-old contact**). `GetStartTime` has no setter in
`G4Scheduler` (`fStartTime = GetNextTime()`, `G4Scheduler.cc:321`).

- Size here: 0 new-old contacts in 448 stock histories with independent tracks
  (`comparison/README.md`, `overlap/README.md`); 10 pairs in the positive control
  (a duplicated packet, 5 nm, 1 ns; `evidence/07-r1-contact-limit-probe.txt`). The
  sweep has almost no power, so the size for overlapping tracks is not known.
- The UHDR example registers the same kernel classes, so it shares the gap
  (`examples/extended/medical/dna/UHDR/src/EmDNAChemistry.cc`, line 352 for the transport).

## 2. The start-time trial runs again for the first packet of a history

If the first packet has already been through a trial (for example captured after the
startup contacts), its within-Reff pairs get the start-time trial a second time.
Measured on 64 packets: expected 3.24 reactions (sum of pair probabilities), 4.80 +- 0.27
reactions actually fired at the start time without a mitigation (307 in total)
(`double-trial/README.md`, `mitigation-spike/README.md`). Each such reaction re-runs
every trial (`Prepare()` and `InitializeReactions`, lines 60 to 70 and 366 to 412), so the
count exceeds the sum of probabilities. The workaround here, zeroing the probabilities
around `Run()`, removes all of them (307 to 0). A setter or an explicit "trial only the
tracks present at start" would not need that workaround.

## 3. Reflecting-box fold in `G4ChemReboundTransportation::calculateNextCoordinate`

For a box [-2, 2] um and a proposed coordinate of 7 um the kernel returns 3 um, outside
the box; a correct fold gives -1 um. For the interval [0, 1]: 2.1 gives 1.7, 2.9 gives -0.7, 1.0
gives 0.0, where a correct fold gives 0.1, 0.9, 1.0 (`G4ChemReboundTransportation.cc:322-355`;
correct values in `examples/poisson-track-irt/test/ReflectingFoldTest.cc`). The UHDR example's own helper
(`BoundedBrownianAction.cc`, lines 85 to 93) folds correctly but is used only for H2O
dissociation. The example here uses its own `ReflectIntoInterval` at the history
boundaries; in-box transport still uses the kernel class, in both builds. Unit tests only:
no run exercised an out-of-box fold (`reflectingBoxFolds` was 0).

## 4. Octree finder with exactly coincident molecules

`G4Octree.icc`, `G4OctreeFinder.icc` and `G4DNAMakeReaction.cc` mishandle molecules at the
same position. The patch is `patches/geant4/11.4.3/molecular-finder/coincident-neighbors.patch`;
background in `docs/dose-rate/molecular-finder.md`. All products of one reaction are put
at one point by `G4DNAMakeReaction`, so coincident molecules do occur: 3 in 1 of 16 packets
in the spike (`spike/README.md`). The stock example refuses other coincidences and
exempts products of one startup reaction.

## 5. `GetProbability()` readers (relevant to any gate-like workaround)

In 11.4.3 only two readers exist in `source/processes/electromagnetic/dna`:
`G4DNAIndependentReactionTimeStepper.cc:170` and `G4DNAIRT.cc:332` (classic IRT). The
example's gate is safe only while that holds; re-audit before allowing another version.

## Also noticed (not defects of the example)

- `G4ITReactionChange` stores reactants in a `std::map<const G4Track*, ...>`, so
  `GetTrackA()`/`GetTrackB()` order follows memory addresses; any per-reaction record
  that depends on that order is not reproducible (`spike/README.md`, G1 finding).
- A molecule such as `OH^-` matches both the tracked `OHm` and the bulk tag `OHm(B)`
  (diffusion coefficient 0, never tracked); lookup by name and charge alone is
  ambiguous (the example skips labels ending in `(B)`).
