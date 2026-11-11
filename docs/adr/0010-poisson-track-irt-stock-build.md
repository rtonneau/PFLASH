# poisson-track-irt builds on stock Geant4; patched build kept selectable

_ADR 0010, recorded 2026-11-10 (session 2026-11-10__poisson-track-irt-stock-build)._

<!-- Format adapted from mattpocock-skills' domain-modeling (ADR-FORMAT.md): https://github.com/mattpocock/skills.git -->

`examples/poisson-track-irt` needed three local Geant4 11.4.3 patches (source contacts, molecular finder, rebound fold). It now builds on a stock install by default, with `PTIRT_PATCHED_GEANT4` (checked at configure time) selecting the patched path. Instead of patching, the stock build resolves the **Startup contacts** at capture, uses its own fold at history boundaries, refuses coincident molecules except products of one startup reaction (each marked by a `productGroup`, a group label and not a Geant4 reaction number), measures what the kernel still misses (**New-old contacts**, reactive coincident pairs), gates the first packet's second contact trial to zero, and warns when the box is small against the diffusion length (ratio below 3). Each build captures its own library from the same physics seeds, because a post-startup packet in the patched build would be trialled twice.

This is an engineering change, not a physics validation. The numbers below say how large each stock approximation is in the runs that were made, and which runs were not made.

Evidence root for every path below: `.scratch/tests/2026-11-10__poisson-track-irt-stock-build/` (git-ignored, local; each directory has a README). Geant4 is referred to as the install in `G4_ROOT` (`Geant4_DIR` = `<G4_ROOT>/lib/cmake/Geant4`); no path to an install is recorded here.

## What each patch fixed, and what replaces it in the stock build

The patch files are in `patches/geant4/11.4.3/` (unchanged by this work). Only `timed-injection/` has a README (`patches/geant4/11.4.3/timed-injection/README.md`); `molecular-finder/` has none, and its background is in `docs/dose-rate/molecular-finder.md`.

| patch | what it fixes in Geant4 11.4.3 | stock replacement | residual |
|---|---|---|---|
| `timed-injection/source-contacts-experimental.patch` (`ResetSourceActivations`, `RegisterSourceActivation`) | The stepper trials contacts within the reaction radius only at the scheduler start time. A source that arrives later is never trialled against the molecules already there, and the first source is trialled a second time. The patch registers each source and its activation time so that the later trial happens once. | Capture takes the packet after the Startup contacts (1 ps, first step of positive length). `history` never calls the two methods. | New-old contacts are not trialled (measured, below). The first packet's second trial is gated off. |
| `timed-injection/meso-boundaries-experimental.patch` (`G4ChemReboundTransportation::calculateNextCoordinate`) | The kernel fold of a coordinate outside the box is wrong for some overshoots: for the interval [0, 1], 2.1 gives 1.7, 2.9 gives -0.7 and 1.0 gives 0.0, where a correct fold gives 0.1, 0.9 and 1.0 (`G4ChemReboundTransportation.cc:322-355`; correct values in `test/ReflectingFoldTest.cc`); for a box [-2, 2] um, 7 um gives 3 um instead of -1 um. | The example's own `ReflectIntoInterval` (`include/ReflectingFold.hh`) is used at the history boundaries (`HistoryBoundedBrownianAction`) in both builds. | In-box transport still uses the kernel class `G4ChemReboundTransportation` in both builds, as does UHDR (`EmDNAChemistry.cc:352`), so it has the same wrong cases. Box-size guard below. |
| `molecular-finder/coincident-neighbors.patch` (`G4Octree.icc`, `G4OctreeFinder.icc`, `G4DNAMakeReaction.cc`) | The octree finder mishandles molecules at exactly the same position. | No finder change. Coincident molecules are refused at capture and at injection, except the products of one startup reaction (same non-zero `productGroup`, same packet). | Reactive exempt pairs are counted (`coincidence.reactivePairs`) but not changed. |

## Measured sizes of the stock approximations

1. **Startup contacts (resolved at capture).** In a 16-packet spike (10 keV e-, seeds 1001..1016) the 4 to 7 zero-time steps before the first positive step carry 247 startup reactions, 11 to 22 per packet, all at the 1 ps start time (`spike/README.md`, G1). The stock packet is therefore a different molecule set from the patched packet, and the two builds record different digests (`chemistryStartDigest` differs from `capturedDigest` in stock; for seed 1001, `31328ec573e06d58` against `b962c89426209bcb` in `evidence/05-capture-1001-1016/capture.json`). The stock chemistry seed is `2147483648 - physicsSeed`, which needs a physics seed below 2^30 (`evidence/04-stock-seed-ceiling.txt`).

2. **Coincident molecules.** The spike found 3 coincident molecules at one position in 1 of 16 packets, all products of `e_aq + e_aq -> 2 OH^- + H_2`, and no other coincidence (`spike/README.md`, G2). The ticket-05 capture of the same seed range found one exempt position in each of 3 packets (seeds 1004, 1008, 1011), `reactivePairs` 0 and `coincidentMolecules` 0 in all 16 (`evidence/05-coincidence-seeds-1001-1016.txt`). The implementer reported them as `H_2 + OH` pairs; that evidence file does not list species, so this is not independently confirmed. The seeds differ from the spike (1010) because the stock build captures after the startup contacts, while the spike read the molecules at chemistry start.

3. **New-old contacts (not trialled by the stock kernel).** The diagnostic counts, at each activation, pairs of a fresh and an already-present molecule closer than their reaction radius. Measured: 0 in 128 stock histories of the comparison (`comparison/README.md`) and 0 in 320 stock histories of the overlap sweep at d = 0.005, 0.05, 0.5, 5 and 100 um (`overlap/README.md`). That is a null result with low power: the two packets are independent tracks, the 10 ns delay lets the first packet diffuse, and the contact is evaluated at 10.001 ns. The positive control `HistoryNewOldContactLimit` (a duplicated packet, 5 nm, 1 ns) gives 10 stock pairs and 3 patched pairs (`evidence/07-r1-contact-limit-probe.txt`). So the sweep does not bound the bias at short delays or for overlapping tracks.

4. **First-packet second trial: measured, then gated (user decision).** The first packet is present at the scheduler start time, so the stepper trials its within-Reff pairs a second time, after capture has already trialled them once. Over 64 packets (seeds 3001..3064): 19.77 pairs and an expected 3.24 reactions (sum of the pair probabilities, SEM 0.145, maximum 6.29, 63 of 64 packets above 1; `double-trial/README.md`). The acceptance rule of the plan was a mean of at most 1, so ticket 7 stopped. A mitigation spike on the same 64 packets (`mitigation-spike/README.md`) counted the reactions that actually fire at the start time without a mitigation: 307 in total, 4.80 +- 0.27 per first packet. That is 1.56 +- 0.23 above the sum of probabilities (z = 6.9), so the sum of probabilities is a lower bound. With the gate, or with an O3 sentinel, the count is 0. The species difference ungated minus gated is °OH -2.14 +- 0.50 at 1 ns and -0.81 +- 0.66 at 10 ns (paired over 64 packets).
   - The user chose the **Probability gate** and **Report both**. `StartTrialGate` (`include/StartTrialGate.hh`) sets every `G4DNAMolecularReactionData` probability to 0 around `chemistry->Run()` and restores the exact bits afterwards, also on an exception. Stock builds only.
   - The ledger field `firstPacketSecondTrial` carries both measures: `{packetIndex, pairs, expectedReactions, decision: "gated", gatedReactionData, startZeroStepReactions}`. `expectedReactions` is the sum of probabilities (the prediction of what was suppressed). The counted measure, 4.80 +- 0.27 per first packet ungated, cannot be counted in the gated run, because those reactions never happen; it is quoted from the spike, and the gated run reports its own observation `startZeroStepReactions` (0 when the gate works; 72 data held at 0 for option3).
   - Red check: the same test with the gate removed fails with 10 zero-step reactions at the start (`evidence/07-r1-red-gate-off.txt`; green run in `evidence/07-r1-start-gate-test.txt`).

5. **Stock against patched (ticket 9).** 256 histories (8 clusters of 3 packets, 16 seeds, 3 ms, same chemistry seeds in both builds): of 1552 rows (97 times x 16 species), 0 are DIFFERENT, 303 INCONCLUSIVE, 243 EMPTY and 1006 EQUIVALENT; 199 rows carry a bias flag (137 of them on EQUIVALENT rows). The largest effect is stock `O^-1` about 10% below patched at 1 to 3 ms (1.4 to 1.8 molecules out of about 12 to 17; CI about [-2.8, -0.6]), reported as INCONCLUSIVE. INCONCLUSIVE means the CI crosses a margin edge; it is not agreement. The two builds capture at different stages, so they do not hold identical molecule sets after the first picoseconds, and the cause of the `O^-1` offset was not traced (`comparison/README.md`). Overlap sweep: 0 DIFFERENT rows at every separation (`overlap/README.md`). Margins were not tuned.

6. **Separated sources (stock).** SEPARATED_CHECK_PASS, 1552 of 1552 cells, worst ratio 0.543; both negative controls fail as they should (362 and 560 cells; `separated/README.md`). Only the stock build was put through it.

7. **Tests.** Final ctest: 46 of 46 in the stock tree and 42 of 42 in the patched tree (`evidence/09-ctest-stock.log`, `evidence/09-ctest-patched.log`).

## Conditions that require the patched build

Use the patched build (`docs/` of the example: `docs/re-enabling-patch.md`) when any of these holds:

- the ledger shows `newOldContacts.pairs` above 0 in a stock run, or a stock run exits with `PTIRT_NEW_OLD_CONTACTS` (set `newOldContactLimit` to make that a failure). Short delays between overlapping tracks are the expected case;
- the study needs the packet at chemistry start (before the startup contacts) rather than after them, or needs chemistry seeds not tied to the physics seed (the stock seed ceiling is 2^30);
- reactive coincident molecules are expected (`coincidence.reactivePairs` above 0), or a coincidence other than one startup reaction's products is refused at capture and must be handled instead;
- the Geant4 version is not 11.4.3. The stock CMake guard accepts 11.4.x but only 11.4.3 was audited, and the patches are for 11.4.3 only;
- the box is small against the diffusion length (the guard prints `PTIRT_BOX_GUARD_WARNING` below a ratio of 3). This is a Geant4 in-box transport limit in both builds, not a reason to switch, but the in-box fold is only fixed by the meso-boundaries patch.

## Decisions

- **Separate library per build.** The tag now carries the build mode and the capture stage (`ptirt2_geant4-11.4.3-<stock|patched>_...-<afterStartupContacts|chemistryStart>`). `history` refuses a library from the other build (`HistoryRejectsPatchedLibrary`, `HistoryRejectsStockLibrary`). A stock packet already went through the startup trials, so injecting it in the patched build would trial it twice. Both builds capture from the same physics seeds, and digests agree on the chemistry-start state (24 of 24 packets, `comparison/README.md`; 16 of 16, `overlap/README.md`).
- **`productGroup` exemption.** Packet format version 2 adds a `productGroup` column: 0 for none, otherwise a dense group label 1..G by first appearance in canonical order. Products of one startup reaction share a label, because Geant4 puts all products of a reaction at one point. It is a label for this check, not Geant4's reaction number. Groups are keyed by track address because the product track ID is still 0 in `UserReactionAction` in the first failed attempt; the review could not establish whether that is also true in 11.4.3 source and the keying is correct either way.
- **Second trial: measured, then gated (user decision).** See item 4 above. The plan's "accept while the mean is at most 1" rule was not met (3.24), the user chose the Probability gate and to report both measures.
- **Box guard.** `BoxGuard` compares the half box with `sqrt(6 D t)` of the fastest species (H3O^1 in option3). A ratio below 3 prints a warning and the run continues; the ratio is in the ledger (`boxFoldGuard`, null for an infinite ratio). It is a warning, never a stop.
- **Own fold at history boundaries.** `ReflectIntoInterval` replaces the kernel fold in `HistoryBoundedBrownianAction` in both builds.
- **Version guard.** The stock configure requires Geant4 11.4.x, because the gate is safe only while `GetProbability()` has no IRT_syn reader other than the start-time trial (`G4DNAIndependentReactionTimeStepper.cc:170` in 11.4.3; the other reader, `G4DNAIRT.cc:332`, is classic IRT, which `history` refuses).

## Rejected options

- **Subclassing or copying kernel classes.** The octree finder singleton is split between exe and DLL, and thread-local accessors fail on Windows. A copy would also have to track the kernel.
- **O3 sentinel start.** Investigated in the mitigation spike (`mitigation-spike/README.md`): an inert O3 injected 1e-6 ns before the first packet, so the first packet is no longer present at the scheduler start. It works (307 to 0 zero-time reactions) but needs an extra stage, exemptions in the observer, species output and ledger, moves the scheduler start by 1 fs, relies on O3 never being a reactant in option3 (`G4EmDNAChemistry_option3.cc:403-406`, product only) and draws extra random numbers, so runs are no longer paired with stock. The gate is simpler. It stays the fallback if mutating the reaction table becomes unacceptable.
- **Setting the scheduler start time directly:** fails; `G4Scheduler` has no setter. **Sentinel = ground-state H2O:** fails (lifetime 0, `NoDecayChannel` is fatal). **Sentinel = a (B) species:** fails (bulk reactants). **Sentinel = NoneM:** rejected from source only (it is the partner of the O3^- first-order decay); not run.
- **Accepting the second trial and documenting the bias:** not chosen; the measured excess (4.80 per first packet) is larger than the sum of probabilities.

## NOT RUN / not proven

- Capture-side coincidence refusal (`PTIRT_COINCIDENT_MOLECULES` from `capture`) is not integration-tested; only the history side is (`HistoryRejectsCoincidentPacket`, `HistoryRejectsCoincidentSources`).
- Reactive exempt pairs are only unit-tested. The real exempt pairs seen all have `reactivePairs` 0 (reported as `H_2 + OH`; species not recorded in the evidence).
- Only the fAlive branch of `PTIRT_CAPTURE_STATE` was forced (`evidence/04-forced-failure-status.txt`); the other branches are code-reviewed only.
- Separation of `H3Op` from the bulk tag `H3Op(B)` (and `H2O(B)`, `OHm(B)`) in `SourceInjector.cc` was not checked empirically; only OH- showed the problem and the four history tests that read stock packets cover the rest.
- The patched install's finder and rebound patches are not proven by a dedicated test. The configure check probes only the source-contact API; the patched ctest passes, but the install was inspected only through its stepper header.
- Multithreaded runs and start-time ties are untested.
- Geant4 11.4.x other than 11.4.3 is accepted by the CMake guard, but only 11.4.3 was audited for `GetProbability()` readers.
- The overlap sweep has no power at short delays (item 3); histories with real new-old contacts within Reff were not compared between builds.
- Not run in the mitigation spike: histories longer than 10 ns, capture with non-zero packet offsets, first packet at T0 + epsilon, and chemistry lists other than option3 (`mitigation-spike/README.md`). The cause of the 106 outside-Reff start-time reactions of the ungated run was not traced.
- The separated-sources check was not run on the patched build; the table-mutating gate has integration coverage but its table constructor has none beyond that.
- The cause of the stock `O^-1` offset at 1 to 3 ms (about 10%, INCONCLUSIVE) was not investigated.
- Never run: any claim about a dose-rate effect, oxygen, or realistic track overlap.
