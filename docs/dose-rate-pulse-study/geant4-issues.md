# Geant4 11.4.3 issues found in the dose-rate work

Every Geant4(-DNA) problem met while building and running the dose-rate tools of this project,
2026-10 to 2026-11-17. For each one: where it is, what it does, what was done about it and why.
Paths are relative to the Geant4 source root
(`geant4SourcePath` in `.claude/.claude-project.json`, i.e. `${DEV_DIR}/GEANT4/geant4-v11.4.3-source`),
and `source/processes/electromagnetic/dna/` is shortened to `dna/`.

**Status legend:** **PATCHED**: fixed by a local patch in `patches/geant4/11.4.3/`, used by the
production runs. **WORKAROUND**: avoided in project code, Geant4 unchanged. **OPEN**: not
solved. **NOTE**: a limit or quirk of the API, not a bug.

Nothing here has been reported upstream. A draft for issues 1 to 4 is in
`examples/poisson-track-irt/docs/upstream-notes.md` ("prepared, not submitted"). Before
reporting, check each item against the current Geant4 release.

## Summary

| # | Component | Problem | Status |
|---|---|---|---|
| 1 | Octree molecule finder | Coincident molecules are duplicated into all 8 children until memory runs out | PATCHED |
| 2 | IRT_syn time stepper | Contact trial only at the scheduler start time: later tracks never trialled, first track trialled twice | PATCHED (experimental patch) |
| 3 | `G4ChemReboundTransportation` | Wrong reflection when a step overshoots a wall by more than the box, or lands on it | PATCHED |
| 4 | `G4DNAMesh::ConvertIndex` | 32-bit int overflow above 65536 cells per side | WORKAROUND (cap) |
| 5 | `G4DNAEventScheduler::Voxelizing` | Radiolytic O₂ left alive at the hand-over; IRT_syn then loops forever | WORKAROUND |
| 6 | `G4DNAEventScheduler::RecordTime` | Records only after a reaction step; late counts stale | WORKAROUND |
| 7 | `G4DNAEventScheduler` re-entry | Re-voxelizing loses molecules; time and empty-queue handling unfit for recurring sources | OPEN (NO-GO for meso dose-rate) |
| 8 | Molecule counter | `TIME_DONT_MATCH` / `RETURN_TO_THE_FUTUR` crash above about 8.6 s | OPEN |
| 9 | `G4DNAScavengerProcess` | Products have no parent; reactions bypass `UserReactionAction` | WORKAROUND (position matching) |
| 10 | IRT_syn neighbour finder | No periodic images: periodic chemistry boundaries miss reactions | WORKAROUND (reflecting box) |
| 11 | `G4Scheduler` | No setter for the start time | NOTE |
| 12 | Windows DLL / TLS | Finder singleton split between exe and DLL; subclassing kernel classes fails | NOTE |
| 13 | `G4DNAEventScheduler::InitializeInMesh` | `WrongResolution` warning when a reaction radius ≥ cell / π | NOTE (benign here) |
| 14 | Process ordering | `ProcMan113` warning: "Set Ordering First is invoked twice for ChemReboundTransportation" | NOTE (not investigated) |
| 15 | Reproducibility | Same seeds, rebuilt binary: different realisation | OPEN (cause not traced) |

---

## 1. Octree finder duplicates coincident molecules (PATCHED)
Tested on its own (fixture and leave-one-out histories): [patch-necessity.md](patch-necessity.md).

- **Where:** `dna/management/include/G4Octree.icc`, `G4OctreeFinder.icc`; contact guard in
  `dna/models/src/G4DNAMakeReaction.cc`.
- **Symptom:** two molecules at exactly the same point work. Three or four exhaust memory
  (256 MiB cap in the tests) during tree construction.
- **Cause (verified in source and by trace):**
  - `G4DNABoundingBox::contains` includes both endpoints, so a point on a split plane belongs to
    every child that touches it.
  - The octree copies each entry into every child that contains it. Identical points can never
    be separated, so each level copies them again.
  - `++current_depth` also leaks across siblings. The tree is finite (max depth 100) but
    combinatorially huge: T(d) = 1 + Σᵢ₌₁..₈ T(d+i).
  - Related defects found on the way:
    - a zero-radius expanding query overflows the stack;
    - centre/half-width pruning loses endpoint inclusion for a near-ULP cloud;
    - `G4DNAMakeReaction` aborts (`G4DNAMakeReaction003`) on a zero-distance pair.
- **Why it matters:** `G4DNAMakeReaction` puts all products of a reaction at one point, so
  coincident molecules do occur: 3 in 1 of 16 packets of a spike (e⁻aq + e⁻aq → 2 OH⁻ + H₂).
- **Fix:** `patches/geant4/11.4.3/molecular-finder/coincident-neighbors.patch`.
  - One owner per entry (equality goes to the upper half); all-coincident populations go into a
    vector leaf.
  - Depth is passed per sibling, and pruning uses the clamped stored bounds.
  - Distance-zero results are allowed; only the querying `G4Track*` itself is excluded.
  - A narrow zero-distance return in `MakeReaction`.
- **Validation:** 55/55 capped cases (14 oracle/structure, 39 native contact, 2 retained
  guards). The 24 ordinary controls are identical to the unmodified kernel. Peak memory was 11.5 MiB.
- **Details:** `docs/dose-rate/molecular-finder.md`. Evidence:
  `.scratch/tests/2026-11-08__molecular-finder-degenerate-coordinates/`.
- **Upstream:** a clean, self-contained candidate. The patch touches 3 files.

## 2. IRT_syn trials contacts only at the scheduler start time (PATCHED, experimental)
Tested on its own (fixture and leave-one-out histories): [patch-necessity.md](patch-necessity.md).

- **Where:** `dna/models/src/G4DNAIndependentReactionTimeStepper.cc`, lines about 167–173: a pair
  already inside its effective reaction radius is trialled only while
  `GetGlobalTime() == GetStartTime()`.
- **Symptom, two-sided:**
  - A track injected later (delayed track) is never trialled against the molecules already
    present (a **new-old contact**). In a dose-rate run every new track lands among older molecules.
  - The first packet, already trialled once at capture, is trialled a second time at the history
    start. Measured on 64 packets: **4.80 ± 0.27 extra reactions** per first packet. That is
    above the sum of the pair probabilities, 3.24, because each reaction re-runs every trial
    (`Prepare()`/`InitializeReactions`, lines 60–70 and 366–412).
- **Fix:** `patches/geant4/11.4.3/timed-injection/source-contacts-experimental.patch` adds two
  static methods.
  - `ResetSourceActivations()` clears the registrations at the start of each history.
  - `RegisterSourceActivation(trackID, activationTime)` registers a source with its activation
    time. Later contact trials then need a source-eligible reactant at its activation time.
  - Old-old contacts keep the original condition. Same-time descendants inherit eligibility
    through the parent ID.
- **Caveat:** the patch README calls it "not production-ready". Pending clocks across scheduler
  stop boundaries and a transactional batch API are not implemented. The 2026-11-15/16 production
  runs nevertheless use it. Its controls (contact, interruption, empty source, bulk continuity,
  scoring, synthetic Serial/MT histories) pass; there is no full recurring-source validation.
- **Stock workaround tried and reverted:** gate all reaction probabilities to 0 around
  `chemistry->Run()` and resolve startup contacts at capture (ADR 0010). It was reverted
  on 2026-11-14 because new-old contacts stay untrialled and are only measured.
- **Upstream suggestion:** a public way to trial "only the tracks that have just become active"
  at a delayed time, or a settable start time (see 11).
- **Details:** `patches/geant4/11.4.3/timed-injection/README.md`,
  `docs/dose-rate/recurring-chemistry-sources.md`, `docs/adr/0010-poisson-track-irt-stock-build.md`.

## 3. Wrong reflection in `G4ChemReboundTransportation` (PATCHED)
Tested on its own (fixture and leave-one-out histories): [patch-necessity.md](patch-necessity.md).

- **Where:** `dna/processes/src/G4ChemReboundTransportation.cc:322-355` (`calculateNextCoordinate`).
- **Symptom:** on the interval [0, 1]:

  | raw coordinate | stock result | correct |
  |---|---|---|
  | 2.1 | 1.7 (outside) | 0.1 |
  | 2.9 | −0.7 (outside) | 0.9 |
  | 1.0 | 0.0 | 1.0 |

  For a box [−2, 2] µm, 7 µm gives 3 µm (outside) instead of −1 µm.
- **Who is exposed:** the Geant4 UHDR example uses this class for the in-box transport of every
  molecule except H₂O (`examples/extended/medical/dna/UHDR/src/EmDNAChemistry.cc:352`). Its own
  `BoundedBrownianAction` (lines 85–93) folds correctly, but only for H₂O dissociation.
- **Fix:** `timed-injection/meso-boundaries-experimental.patch` rewrites `calculateNextCoordinate`
  as a fold over a period of twice the box length (`std::fmod(nextPos - low, 2·length)`). It is
  fatal on a non-finite coordinate. That patch also carries the experimental persistent-mesh
  scheduler changes of issue 7.
- **Impact here:** `examples/dose-rate-study` has **no fold of its own**. `DrsPhysicsList::InstallReflectingBox`
  replaces the Brownian transport of every molecule except H₂O with `G4ChemReboundTransportation`.
  The H₂O dissociation action (`HistoryBoundedBrownianAction::Fold`) calls the kernel's
  `calculateNextCoordinate`. Correct reflection therefore depends on the patch. With a stock
  kernel, a step overshooting a wall by more than the box would put the molecule outside the
  box. Whether a run would then stop or continue silently was not tested. The ledger field `reflectingBoxFolds` counts the
  dissociation-product folds: e.g. 24 in `plan_pulse_rx/r10000_000`, 0 for reaction products.
- **History:** the stock-build attempt (ADR 0010, reverted) had its own `ReflectIntoInterval`
  and the test `examples/poisson-track-irt/test/ReflectingFoldTest.cc` with the correct values.
  Both were removed by the revert (commit `745951b`) and remain in git history (`e925b0d`).
  Draft upstream text: `examples/poisson-track-irt/docs/upstream-notes.md` §3.

## 4. `G4DNAMesh::ConvertIndex` 32-bit overflow (WORKAROUND)

- **Where:** `dna/models/src/G4DNAMesh.cc:214-225`. `(G4int)(index.x * pixels / xmax)` multiplies
  two `G4int`s.
- **Symptom:** fatal `G4DNAMesh013` at the first coarsening when the mesh starts above 65536 cells
  per side.
- **Workaround:** `MesoSettings::PixelCount` caps the pixel count at 65536, with a `DnaLogger`
  warning. On the default 1 mm box this gives 15.26 nm cells instead of the requested 6.25 nm
  (ADR 0006).
- **Upstream fix:** trivial, a 64-bit intermediate (`G4long`/`int64_t`).
- **Not used** by the dose-rate study, which has no mesoscopic stage. It concerns the main
  `sim` application.

## 5. Radiolytic O₂ left alive at the mesoscopic hand-over (WORKAROUND)

- **Where:** `G4DNAEventScheduler::Voxelizing`. The scavenger material holds O₂, so the voxelizer
  skips O₂ tracks produced by radiolysis.
- **Symptom:** IRT_syn later pairs them with killed tracks and `FindReaction` loops forever, so the run
  hangs at the hand-over.
- **Workaround:** commit `e0a5b88`. Radiolytic O₂ is merged into the bulk O₂ pool and killed before
  voxelizing (`docs/irt-syn-mesoscopic-validation.md`, ADR 0006).

## 6. Mesoscopic records only after reaction steps (WORKAROUND)

- **Where:** `G4DNAEventScheduler::RecordTime`, called from a reaction step only. It records at
  most one boundary per call.
- **Symptom:** when reactions become rare, the record times after the last reaction keep stale
  counts. With bulk O₂, e⁻aq stopped decaying from about 800 ns.
- **Workaround:** commit `baf4b29`. The project's mesh action records `SpeciesMeso` after every
  Gillespie step. O₂ e⁻aq vs SBS went from +159 % to +9 %.

## 7. Mesoscopic scheduler cannot take recurring sources (OPEN, NO-GO)

- **Where:** `G4DNAEventScheduler` (`Voxelizing`, `Stepping`, `RunInMesh`, `Reset`), source audit
  in `docs/dose-rate/mesoscopic-feasibility.md`:
  - A second `Voxelizing` assumes a homogeneous old mesh and drops molecules: 8 OH in a cell
    before, 0 after (measured).
  - `Stepping` reads the next event time beyond the end time; `RunInMesh` advances the clock to
    it even if the event did not run. With an empty event set the outer run can fail to advance.
  - The scheduler constructor leaves the recording iterator uninitialised. This caused access
    violations in a packet-cycle ensemble.
  - `G4DNAGillespieDirectMethod` does not run at-rest molecular processes. A 1 ps packet
    (pre-dissociation H₂O) therefore cannot be voxelized directly.
  - The mesh has no outward diffusion neighbours.
- **Status:** an experimental private scheduler (`meso-boundaries-experimental.patch`,
  `meso-bulk-instrumentation-experimental.patch` = diagnostics only) passed bounded controls.
  Accuracy was not established, so the decision was **NO-GO** for a mesoscopic dose-rate
  application. The dose-rate study therefore runs IRT_syn only, up to beam-off.
- **Why not fixed:** it needs an ordered absolute timeline, state snapshots at observation times,
  bulk continuity and global propensity invalidation. That is a redesign, not a patch.

## 8. Molecule counter crash above about 8.6 s (OPEN)

- **Where:** `G4VUserMoleculeCounter::AddMolecule` (`TIME_DONT_MATCH`) and `RemoveMolecule`
  (`RETURN_TO_THE_FUTUR`), comparer `G4MoleculeCounterTimeComparer.cc`.
- **Symptom:** two 0.1 Gy/s, 1 Gy histories aborted, at 8.79 s and 9.78 s. The two times printed
  in the message are equal.
- **Verified in source:** the check fails when a new time is earlier than the last recorded
  counter time by more than the comparer precision.
- **Project factor:** the dose-rate example sets that precision to **1 fs**
  (`kCounterPrecisionNs`, `examples/dose-rate-study/src/HistoryMain.cc`). The Geant4 default is 1 ps.
- **Hypothesis (not reproduced):** times are absolute doubles in ns. Above 2³³ ns = 8.59 s, one ulp
  is 2⁻¹⁹ ns ≈ 1.9 fs, larger than the 1 fs tolerance. Two times one rounding step apart then fail
  the check. Both crashes are above 8.59 s, but 6 of 7 histories passed that point.
- **Candidate fixes, untested:**
  - relative tolerance in the comparer (Geant4 side);
  - raise `kCounterPrecisionNs` to ≥ 1 ps (project side);
  - measure from a later time origin.
- **Impact:** one 0.1 Gy/s production run excluded (6 of 7 used). The pulse runs at 0.1 Gy/s stop at
  0.1 Gy (≤ 1.2 s), far from the limit.

## 9. Scavenger (bulk) reactions are invisible to `UserReactionAction` (WORKAROUND)

- **Where (verified in 11.4.3 source):**
  - `dna/processes/src/G4DNAScavengerProcess.cc:281-300` (`PostStepDoIt`) builds each product with
    `pProduct->BuildTrack(reactionTime, track.GetPosition())` and pushes it to the track holder.
    It never calls `SetParentID`, so the product's parent ID is 0. The reactant is killed with
    `fStopAndKill` (line 369).
  - `UserReactionAction` is called only in `dna/management/src/G4ITModelProcessor.cc:219-223`,
    for pair (bimolecular) reactions.
- **Symptom:** first-order bulk reactions (pH buffer `H3Op(B)`/`OHm(B)`, bulk O₂) leave no trace in
  the user reaction hook. Their products cannot be linked to the molecule they came from.
- **Why it matters:** the per-track reaction log needs the origin of every molecule. At pH 7 the
  bulk channel e⁻aq + H₃O⁺(B) is a major e⁻aq sink: 0.26 to 0.46 per 100 eV of a track.
- **Workaround:** `ReactionLog::ObserveStep`, called after each time step.
  - Every molecule that vanished without a pair reaction is logged as a `bulk` reaction
    (source −2).
  - A parentless new molecule is attributed to the nearest such vanished molecule.
  - Pre-chemical water (`H2O^0`, `H2O^+`) is excluded.
  - Result: 0 molecules of unknown origin in 12 runs. Matching was needed for 654–6923 products
    per run.
- **Upstream suggestion:** set the parent ID of the products to the reactant's track ID, and call
  a user hook (or a separate `UserScavengerReactionAction`) from `G4DNAScavengerProcess`.

## 10. No periodic images in the IRT_syn neighbour finder (WORKAROUND)

- The study was asked for periodic chemistry boundaries "as in the UHDR example". UHDR's periodic
  condition applies only to physics-stage electrons; its chemistry reflects molecules.
- Under IRT_syn the finder has no periodic images. A periodic wrap would silently miss reactions
  between molecules close across a face, and a minimum-image finder would need another kernel patch.
- **Workaround:** reflecting walls (a mirrored lattice), ADR 0011.

## 11. `G4Scheduler` start time cannot be set (NOTE)

`fStartTime = GetNextTime()` (`G4Scheduler.cc:321`), no setter. This blocks the simplest fix of
issue 2's double trial. An inert O₃ sentinel injected before the first packet works (307 → 0 zero-time
reactions) but was rejected as too invasive (ADR 0010).

## 12. Kernel classes cannot be subclassed or copied on Windows (NOTE)

The octree finder singleton is split between the executable and the DLL, and thread-local accessors
fail across the DLL boundary. Fixes must therefore be patches to the Geant4 source, rebuilt into
`G4processes_core.dll`. Copying headers is not enough (ADR 0010, `molecular-finder.md`).

## 13. `WrongResolution` at mesh initialisation (NOTE)

`G4DNAEventScheduler::InitializeInMesh` warns when a reaction radius is at least cell size / π
("resolution is not good : 15.258789", once per event). The observed pair is O⁻ + H₃O⁺. Gillespie
propensities use counts, rates and volume, not that radius, so the warning is known benign at
15.26 nm cells. Do not assume every resolution warning is benign.

## 14. `ProcMan113` warning on ChemReboundTransportation (NOTE, not investigated)

Every `history` run starts with repeated `G4ProcessManager::SetProcessOrderingToFirst()` warnings,
"Set Ordering First is invoked twice for ChemReboundTransportation to H" (and other species), in
`${STUDIES_ROOT}/dnachem-min/2026-11-16__pulse-by-pulse/plan_quick/det.log`. It is a warning only. Whether
Geant4's chemistry constructor or this project registers the ordering twice was not checked.

## 15. A rebuilt binary gives a different realisation (OPEN, cause not traced)

- The same `history.exe` with the same config and seeds gives the same result (repeat test in
  `plan_quick/det`).
- After rebuilding `history.exe` with the reaction log, the same configs and seeds gave the same
  arrivals (same sources and end time) but a slightly different chemistry: e.g. `r50_002` with
  56750 vs 56866 IRT steps (`plan_pulse` vs `plan_pulse_rx`).
- **Possible causes, none verified:**
  - the added observer changes floating-point evaluation order or random-number consumption;
  - compiler code generation changes.
- Earlier, `docs/dose-rate/mesoscopic-feasibility.md` saw a same-seed packet run differ between
  two builds, and even between repeats on one instrumented core.
- **Impact:** comparisons between `plan_pulse` and `plan_pulse_rx` are statistical, not paired.

## Not Geant4 issues (for completeness)

- SBS inherited the partially diffusion-controlled reaction type 1 meant for IRT_syn: a
  project bug, fixed (`docs/sbs-regression.md`).
- IRT_syn alone gives +43 % H₂O₂ vs SBS in water with the same reaction table: a model difference,
  open question, not a defect (`docs/irt-syn-mesoscopic-validation.md`).
- `G4DNAScavengerProcess`'s bulk population getter returns a floored molecule count despite its
  `PerVolumeUnit` name (`docs/dose-rate/restricted-coupling-feasibility.md`): naming only.
