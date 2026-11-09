# Particle reference — statistical validation of the isolated adapter

Ticket 2 remains **not accepted** after implementing and testing an isolated
source-contact candidate. The present blocker is **baseline chemistry bias in the
copied-kernel adapter even with its source-contact correction disabled**. This is
separate from the earlier synthetic delayed-contact regression. Ticket 3 has no
validated particle reference yet. These findings reject this adapter; they do not
establish that timed injection is physically impossible.

The latest protocol accepts statistical equivalence, not identical trajectories.
The historical contact experiment below is retained as evidence, followed by the
resumed ensemble study and its baseline isolation results.

## Experiment fixed before acceptance

The isolated `InjectionReference` executable uses the existing project geometry,
physics and PureWater chemistry, standalone chemistry initialization, and the
unmodified installed Geant4 kernel. No production or shared Geant4 source is edited.
It is currently a focused gate, not the complete reference harness.

The synthetic fixture has a distant O2 spectator at x=100 um and a pair of hydrated
electrons from distinct diagnostic packet IDs separated by 0.01 nm near the origin.
These are controlled molecular sources, not calibrated 500 keV packets. The code
asserts the initialized electron/electron reaction probability is 1 and its effective
radius exceeds the separation. Measured radius: 0.134838 nm. Thus the contact pair
is expected to react on activation, before diffusion separates it.

Each process evolves one shared chemistry system to 20 ps. Ancestry is external to
Geant4 and used only for diagnostics, leaving reaction selection unchanged. The
main-list stage trace and pre-step/reaction checks reject premature activation.
The spectator establishes an existing bath; no scheduler restart occurs at injection.

Controls and final observed results:

- Simultaneous: spectator and electron pair activate at 1 ps. One cross-packet
  reaction occurs at 1 ps; H2 and two OH- products survive with the spectator.
- Shifted startup: all three activate at 10 ps. The same cross-packet reaction
  occurs at 10 ps. Absolute activation time alone does not cause the failure.
- Delayed: spectator activates at 1 ps; the same electron pair activates at 10 ps.
  Both electrons remain at 20 ps, with zero cross-packet reactions. This fails for
  seeds 1729, 1733 and 1741. The stage trace shows the electrons enter the main list
  at 10 ps, with no premature activation.
- Nonreacting: substitute O2 for the two electrons, keeping delayed timing. The
  original spectator and both additions survive, with no premature activation or
  reaction. Population accounting passes.

Final CTest result: 3 pass, 3 fail, 1.10 s. The three delayed tests are deliberately
real failing checks, not WILL_FAIL, skipped, or converted into expected successes.
An initial diagnostic used exactly coincident coordinates and its startup control
also failed; replacing that degeneracy with finite 0.01 nm separation yielded a
valid startup control. No conclusion relies on that discarded coincident fixture.

## Source explanation

Read directly under the configured source tree (environment-resolved path):

- `source/processes/electromagnetic/dna/management/src/G4Scheduler.cc`,
  `SynchronizeTracks`, lines approximately 380–405: merges the next delayed time,
  retains old main-list tracks, and limits processing to the next activation.
- The same file, `Stepping`, approximately 629–638: on a stop-time boundary it marks
  an interaction step and clears scheduled reactions.
- `source/processes/electromagnetic/dna/models/src/G4DNAIndependentReactionTimeStepper.cc`,
  `Prepare`, approximately 60–69: interaction steps clear/reinitialize reaction
  state and rebuild the chemical finder.
- The same file, `CalculateStep`, approximately 165–176: when distance is below
  the reaction radius, zero-time contact reaction is sampled only if scheduler
  global time equals scheduler start time. A later source activation is outside
  this condition even though the newly introduced pair is already within contact.
- `source/processes/electromagnetic/dna/utils/src/G4DNAMolecularReactionTable.cc`,
  `ComputeEffectiveRadius`, approximately 113–134: radius construction and default
  contact probability 1. The fixture checks those actual initialized values.
- Project `src/chemistry/catalog/PureWaterReactions.cc`: e_aq + e_aq is the type-0
  reaction with H2 and two OH- products used by the passing startup controls.

The measured timing dependence agrees with this source restriction. It establishes
that blindly preloading delayed packets into the installed IRT_syn scheduler is
not sufficient for the required reference. It does not establish that timed
injection is impossible with a corrected kernel or another validated design.

## Required recovery and limits

A source-aware contact treatment is needed: identify genuinely new molecular
instances at an activation, account for new-new and new-old contact reactions,
and preserve valid old-old conditional reaction history and bulk/scoring state.
Blindly resetting the scheduler start time or removing the condition for every
old overlapping pair would change unrelated chemistry and is not a validated fix.
An isolated kernel patch plus those controls is required before this reference can
support ticket 3. No such patch has been claimed correct or applied to the shared
installation. This is a scientific validation dependency, not an access denial.

Still unimplemented/unvalidated: the full packet schedule/import history driver,
matched direct-generation ensemble comparisons, calibrated Poisson histories,
library-versus-history uncertainty, all beam-end/empty/multiple-packet fixtures,
Serial/MT independence, yield curves and population scaling measurements. No
readiness or production-feasibility conclusion can be drawn from this gate alone.
Ticket 3 remains pending, rather than being marked complete with a speculative
mesoscopic model. Ticket 1 and original checkpoint evidence remain unchanged.

## Reproduce

Use the existing environment resolution and MSVC x64 build instructions. Configure
`test/timed-injection/probe` into
`.scratch/tests/2026-11-06__timed-track-injection-dose-rate-feasibility/02-msvc`
with Ninja, RelWithDebInfo, the configured Geant4_DIR and vcpkg toolchain; build
only target `InjectionReference`. With Geant4 and vcpkg DLL directories on PATH:

    ctest --test-dir <scratch>/02-msvc -L injection-reference --output-on-failure --no-tests=error

Expected on the current install: nonzero CTest exit, delayed-contact failures.
Individual runs take `InjectionReference <Capture.in> simultaneous|shifted|delayed|nonreacting [seed]`.
Artifact logs: `02-msvc/contact-final.log` and `02-msvc/Testing/Temporary/LastTest.log`.
The pre-existing untracked build directory was not reused or cleaned. No completion
commit is made while acceptance fails; the focused probe and report remain local
for the corrective implementation.

## Resumed statistical protocol (before realistic comparisons)

The user approved statistical chemistry agreement on 2026-11-07. Preserve the
contact regression but do not infer production bias from it alone. First compare
copied-kernel baseline and source-contact candidate on identical unmodified packet
clouds at 3 ps after the last activation, with 16 chemistry seeds per condition.
Use 8 existing independent physical packets, two seeds each; report both within-
packet variation and variation across packet means. Main observables: hydrated
electron, OH, H2O2, H2 and total surviving population. Report paired mean differences
and 95% intervals; an interval overlapping zero is not proof of equivalence. For
this bounded pilot, use a provisional 5% relative equivalence margin for abundant
species and 1 molecule per incident packet absolute margin for rare species, taking
the larger. Report inconclusive results when the interval exceeds the margin.
This is an exploratory accuracy choice, not production approval.

The candidate handles contacts involving registered source tracks only, on their
exact activation time; it does not reset the scheduler origin or resample unrelated
old-old contacts. Generated source copies retain Geant4 license notices. The
candidate does not yet register unimolecular dissociation-product ancestry, so
cross-reaction diagnostics are incomplete and cannot accept ticket 2 yet.

The first 32-history impact assay (8 packets x 2 seeds x baseline/candidate) is
saved under 02-msvc/ensemble. It is not an equivalence acceptance comparison:
baseline is the known problematic delayed path. The first candidate changes OH-
yield materially and leaves some rare-species comparisons inconclusive. This
shows that the contact issue cannot simply be declared negligible from abundant
species agreement. Candidate v2 includes source-born unimolecular/bimolecular
products at the exact source activation time, using the parent IDs documented in
G4ITStepProcessor2.cc and G4ITModelProcessor.cc. Old-old contact rules are unchanged.
Next compare v2 natural startup against the same packet arriving into a distant
noninteracting spectator bath; subtract that known spectator from O2 yields.
This time-origin invariance control uses the same provisional statistical margins.


## Baseline isolation and decision, 2026-11-07

The v2 candidate passes all four contact fixtures (simultaneous startup, shifted
startup, delayed contact and nonreacting population accounting). Its time-origin
control uses 8 independent physical packets and 2 chemistry seeds per packet per
condition, 32 histories total, ending at absolute 3 ps. Abundant species agree
within the predefined pilot margins, but H2O2, OH-, O- and atomic O do not all
establish equivalence. The result is **inconclusive**, not acceptance. Saved under
`02-msvc/invariance-v2/comparison.txt`.

A stricter prerequisite uncovered a much larger problem: natural startup using
the copied kernel does not reproduce natural startup using the installed kernel.
`PacketHistoryInstalled` and `PacketHistory` use the same driver, input packets,
geometry, chemistry, timestamps and endpoint (absolute 2 ps). No spectator,
no delayed second packet, and **contact correction disabled in both**. The copied
stepper preserves the original TLS minimum-step assignment through the exported
`G4VITTimeStepComputer::SetTimes` accessor; removing that assignment was separately
checked and is not an adequate explanation for the bias. Matching the installed
kernel's C++17 compilation of the copied source also did not remove the bias;
that unsuccessful compiler-mode experiment was reverted.

For 8 independent packets, 2 new chemistry seeds each, 32 process runs total:

| Observable, molecules per incident packet | Installed mean | Copied mean | Paired mean difference, 95% interval | Pilot margin |
|---|---:|---:|---:|---:|
| Hydrated electron | 4016.25 | 4944.56 | +928.31 [655.30, 1201.33] | 200.81 |
| H | 815.19 | 179.38 | -635.81 [-829.45, -442.18] | 40.76 |
| H2 | 229.19 | 94.06 | -135.13 [-176.00, -94.25] | 11.46 |
| H2O2 | 29.25 | 13.94 | -15.31 [-19.14, -11.49] | 1.46 |

These intervals lie outside the predefined equivalence regions. The hydrated
electron shift is about 23%; increasing chemistry-history count cannot reasonably
turn this into a 5% equivalence result. The confidence intervals use independent
packet clusters (df=7, t=2.365); two chemistry replicates are averaged within each
packet. Between-packet baseline SD includes residual chemistry noise and must not
be described as a pure estimate of library variance. No familywise simultaneous
coverage is claimed. Source counts, mass and deposited-energy fields are identical
between paired runs. Artifact directory: `02-msvc/kernel-parity`.

The original ticket-1 **direct-generation** results at 2 ps provide an independent
bridge. Comparing the same candidate results with those saved direct counts gives
hydrated-electron difference +909.06 [641.98,1176.14] against margin 201.78, and
H2O2 difference -17.06 [-20.23,-13.89] against margin 1.55. These too fail statistical
equivalence. This bridge has one original direct chemistry realization per packet,
compared with the mean of two new candidate histories; repeated direct rows in the
comparison input are bookkeeping, **not independent extra direct samples**.
Artifact directory: `02-msvc/direct-parity`; original evidence remains untouched.

The narrower isolation run on packet 0 with seed 50001 also confirms that the
installed-kernel history driver agrees closely with the original direct/import
results, whereas the copied kernel shifts the yields with the correction both
on and off. Files: `installed-check.txt`, `clone-check.txt`, `tls-check.txt`, and
`cxx17-check.txt`. This isolates the failure to the copied-kernel integration;
it does not identify the final internal cause. The source/install stepper and
reaction-set headers have matching SHA256 hashes; the installed DLL and configured
build DLL have matching sizes/timestamps. No shared source or installation changed.

**Decision:** reject the isolated adapter as a particle reference. The blocker is
a demonstrated failure of statistical baseline equivalence, not an exact-trajectory
gate and not merely missing implementation. The contact correction cannot be
validated on top of this biased baseline. Ticket 2 is not shipped and ticket 3 is
not started. This does not establish a physical NO-GO for the requested beam.
Remaining acceptance work (multiple realistic schedules, bulk-state checks,
Serial/Geant4-MT independence, time-series and memory/scaling) remains explicitly
unfinished, and is not itself the reason for this rejection. A next implementation
must first obtain statistical parity for the copied kernel with the correction
off, then validate the source correction and finish those controls. No widening of
tolerances, homogeneous-source substitute or independent-cloud merge is justified
by these results.

### Reproduction of the statistical failure

Build `PacketHistory`, `PacketHistoryInstalled`, `CompareInjectionResults`, and
`InjectionContactCandidate` in the isolated MSVC scratch build. Configure
`PROBE_G4_SOURCE` to the resolved configured source path. Use the intact ticket-1
`01-msvc/validation` directory as the packet library. With the configured Geant4
and vcpkg DLL directories on PATH:

```powershell
cmake -DEXE=<build>/PacketHistory.exe -DINSTALLED=<build>/PacketHistoryInstalled.exe `
  -DMACRO=<repo>/test/timed-injection/probe/Capture.in `
  -DLIBRARY=<scratch>/01-msvc/validation -DOUT=<new-artifact-directory> `
  -DCOMPARE=<build>/CompareInjectionResults.exe `
  -P test/timed-injection/probe/RunKernelParity.cmake
```

The comparator returns **3** for failed equivalence; the CMake driver then fails.
For CTest registration configure `PROBE_PACKET_LIBRARY` to that library directory
and run `ctest -R InjectionKernelParity --output-on-failure`. It is a real failing
check, without WILL_FAIL or skipped-success treatment. Existing baseline contact
failures remain visible under `injection-reference`; candidate contact successes
are separately labelled `injection-candidate`. Report files contain all species,
total population, cluster intervals and within-packet paired variability.

## Private-library continuation

The subsequent private build, finder diagnosis, source-contact implementation,
clustered chemistry checks, real schedules and native Serial/MT validation are
recorded in [recurring-chemistry-sources.md](recurring-chemistry-sources.md).
The adapter failure above remains valid historical evidence; it is not the
current unchanged-library baseline. Use the current status and artifact inventory
in that report. The subsequent private-DLL validation passes the bounded ticket-2
controls and statistical comparisons, with the documented reuse and provenance
limitations. Its explicit readiness decision permits matched ticket-3 experiments;
it does not establish full-dose feasibility or production validity.
