# Precursor packet and calibration validation — ticket 1

The isolated probe implements a reusable initial state, not arbitrary-time
checkpointing. It uses the configured Geant4 11.4.3 project physics, PureWater
chemistry and 0.5 mm full-side water cube. Production sources and the shared
Geant4 installation are unchanged: CMake instruments copies in its build directory.

## Physical boundary and source audit

Capture is in `TimeStepAction::StartProcessing`, before delayed-track synchronization
in `G4Scheduler::Process` (source `source/processes/electromagnetic/dna/management/src/G4Scheduler.cc`,
roughly lines 276–360). This is the 1 ps activation boundary: transport has ended,
but molecular dissociation, electron-hole recombination and diffusion have not run.
It is a precursor cloud, not a radical-only cloud.

The configured `.cc` sources were read directly:

- `dna/utils/src/G4DNAChemistryManager.cc`, CreateWaterMolecule/CreateSolvatedElectron:
  modified H2O uses full electronic occupancy, status fStopButAlive and 1 ps;
  solvated electrons are alive at 1 ps. No pulse delay is used in this fixture.
- `dna/molecules/management/src/G4Molecule.cc`, BuildTrack: samples momentum,
  constructs a fresh dynamic particle/track and reconstructs its touchable from
  position. Import restores momentum, kinetic energy, status and all three clocks.
- `dna/molecules/management/src/G4MolecularConfiguration.cc`: names alone are not
  electronic identities. The import resolves definition, charge, full occupancy
  and configuration decay time against the initialized configuration inventory.
- `dna/processes/src/G4DNAMolecularDissociation.cc`: decay time minus proper time
  controls dissociation; products start at the parent's global time.
- `dna/processes/src/G4DNAElectronHoleRecombination.cc`: spatial/configuration
  selection, including charge and material temperature/density, is relevant at
  startup. Transport parent IDs are not the chemical identity.
- `dna/management/src/G4ITTrackHolder.cc`: Push assigns IDs only to ID-zero tracks.
  Imported instances use distinct positive IDs above 1,000,000, checked after Push;
  Geant4's generated chemistry IDs decrease from its nonpositive counter.
- `G4DNAChemistryManager::HandleStandaloneInitialization`: standalone imports must
  build molecular physics tables and close geometry. `UseAsStandalone(true)` is
  necessary; calling Run without this caused a startup access violation.

The UHDR primary-generator example establishes uniform square-face entry with
parallel direction; chem6 ActionInitialization establishes event-local molecule
counter setup. The probe follows those patterns while retaining project physics.

## Packet contract

`PFLASH_PRECURSOR 1` stores doubles as decimal text of their IEEE-754 uint64 bit
patterns, avoiding decimal floating-point precision loss. Units are Geant4 internal
mm, ns and MeV, except explicitly stored massKg. Every packet contains the actual
entry position, parallel direction, cube half-side, water mass, deposited energy,
secondary deposited energy and the entire correlated molecular cloud. Every row
contains definition, charge, complete orbital occupancy, status, position, momentum
direction, global/local/proper time, configuration decay time and kinetic energy.
The stable-electron negative decay sentinel is preserved, not treated as a negative age.

Compatibility includes a SHA256 over project source/header content and the fixed
Geant4/model/beam/boundary contract, plus the exact macro contents. The actual cube
and mass are independently checked on import. Different geometry, chemistry macro,
source fingerprint, schema version, unknown configurations, invalid occupancies,
invalid/nonfinite numbers, unsupported timing/status, oversized counts, truncation
and trailing garbage are rejected. Every configuration is resolved before creating
any track. Source transport IDs are deliberately not reused; packet ordinal gives
one-to-one provenance. This format is only for this initial boundary and compatible
builds, not a general-purpose Geant4 persistence API.

Zero-deposit and empty incident packets are supported and round-tripped; they must
not be removed from a library. Solvation displacements may cross the cube boundary,
so precursor coordinates are never clipped or arbitrarily translated.

## Beam, scoring and calibration

The C++ source sampler overrides the diagnostic gun position with independent
uniform x/y coordinates over the complete entrance face, z = -250 um, direction
(0,0,1), electron energy 500 keV. Energy cuts are disabled; the existing PrimaryKiller
box is 1 km, beyond the transport world. All deposits from all physical tracks in
water are summed, including every generation of secondaries. Escaping kinetic
energy is summed separately and Edep + Eescape must equal 500 keV within 0.01 eV.
The actual PrimaryKiller cut paths are instrumented, including secondary cuts;
aborted events and cuts are rejected. Physical attachment/solvation kills are
not incorrectly classified as truncation. Rejection is deferred until safe event
completion instead of throwing through the Geant4 stepping stack.

Mass is material density times actual solid volume: 1.2500000000000002e-7 kg.
For energy expressed in MeV, the exact conversion used is 1.602176634e-13 J/MeV:

    lambda = doseRate_Gy_s * massKg / (meanEdep_MeV * joulePerMeV)
    actualDose_Gy = sumEdep_MeV * joulePerMeV / massKg
    exposureDuration_s = nominalDose_Gy / doseRate_Gy_s

Eight independent library incidents and eight separately seeded holdout incidents
are used, with no rejection by deposited energy. Both samples cover all four face
quadrants; ranges and individual energy/cloud records are included below. Sample
standard errors use the unbiased sample variance divided by n. The library mean
is about 96.62 keV (SE 11.17 keV); holdout about 107.05 keV (SE 13.09 keV), a difference
of 0.61 combined standard errors. The illustrative calibration is about 8.07e6
primaries/s per Gy/s, with roughly 11.6% relative standard error from the small
library mean (first-order propagation). This is exploratory precision, not a
production calibration. No full-dose chemistry simulation was launched.

## Controls and interpretation

The protocol and bounds are in `test/timed-injection/probe/Protocol.md`, recorded
before the library comparisons. Eight fresh-process imports require exact field
reconstruction and lossless reserialization; chemistry is then evolved to 2 ps.
Direct and import reset CLHEP to the same chemistry seed after initial construction.
Their trajectories need not coincide because allocation/order/fresh IDs can alter
stochastic pairing. Aggregate species counts are compared with the prespecified
exploratory bound, and one repeated-direct run measures baseline variability.
These controls exercise precursor dissociation and early chemistry, not long-time
chemical equivalence or a production error tolerance. The square-root-count bound
is a diagnostic heuristic, not a confidence interval treating clustered molecules
as independent experimental replicates.

Arrival and whole-packet-selection engines are separate std::mt19937_64 instances;
neither uses CLHEP transport/chemistry randomness. The source emits only arrivals
in [0,T), samples a whole correlated packet by index, and never stops at a measured
dose threshold or rescales yields. Activation is arrival plus stored 1 ps age;
a later ticket must retain activations after T when the primary arrived before T.
100,000 lightweight histories test Poisson counts (lambda*T=8), exponential waiting
times, selection independence and compound-Poisson deposited-energy mean/variance,
including a zero-deposit packet. Invalid calibration means are rejected.

Ticket 2 still requires shared-history timed injection, inactivity before arrival,
cross-packet chemistry and adequate observation horizons. Nothing here establishes
mesoscopic coupling, full-dose feasibility, oxygenated-case accuracy, or arbitrary
live-state restart. Tickets 2 and 3 have not been started.

## Reproduction and artifacts

Resolve paths from `.claude/.claude-project.json` and load MSVC x64 as documented
in `.claude/geant4-instructions.md`. Configure the isolated directory:

    cmake -S test/timed-injection/probe -B <scratch>/01-msvc -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DGeant4_DIR=<install>/lib/cmake/Geant4 -DCMAKE_TOOLCHAIN_FILE=<toolchain>
    cmake --build <scratch>/01-msvc
    ctest --test-dir <scratch>/01-msvc -L injection-packets --output-on-failure --no-tests=error

Here `<scratch>` is `.scratch/tests/2026-11-06__timed-track-injection-dose-rate-feasibility`.
The pre-existing untracked `build-timed-injection-probe/` was preserved; the equivalent
planned CTest label runs in isolated scratch instead. Live tests use the install's
compatible release CRT. The plain C++ packet/source test is also built and run in
`01-unit-debug`; it uses explicit failures, so checks remain active under NDEBUG.

`01-msvc/validation` contains packets, direct/import counts, per-event energy balance,
rejection logs and summary.txt. The native-debug logs document the diagnosed probe
error and are not acceptance evidence. Original checkpoint files and ignored GPS
state remain local and untouched by the ticket commit.

## Final validation evidence (2026-11-07)

All four injection-packets CTests passed on the final implementation in 94.21 s:
PrecursorBoundary, PacketAndSource, LibraryHoldoutChemistry, PacketRejections.
PacketAndSource additionally passed in the separate Debug build (1.04 s).
Fresh-process packet files and their reserializations have identical SHA256 hashes.
All 17 direct physical histories satisfy the energy balance; maximum absolute
residual was below 5.1e-8 eV. Live logs contain no G4Exception/fatal markers.
Negative fixtures return the expected PACKET_ERROR and exit 2.

Final numerical results follow. Repeated-direct counts demonstrate stochastic
variation; initial records and deposited energies are tested losslessly.

```text
library0 Edep_MeV 0.12232085733823851 secondary_MeV 0.096176247338266754 entry_mm -0.1977377726274846 -0.21783401707932315 precursors 13162
library1 Edep_MeV 0.076697700168254848 secondary_MeV 0.054442040168270547 entry_mm 0.047527940313533801 -0.16181031458907963 precursors 8213
library2 Edep_MeV 0.13233971051713811 secondary_MeV 0.10743342051716154 entry_mm -0.0079351181060884346 -0.057719845208283216 precursors 14183
library3 Edep_MeV 0.067156180262338466 secondary_MeV 0.048135050262342577 entry_mm 0.22280181165795232 0.21575798685676528 precursors 7154
library4 Edep_MeV 0.10294205594899071 secondary_MeV 0.077405565949024577 entry_mm -0.05944252970881625 0.097604353453276504 precursors 11022
library5 Edep_MeV 0.13943316414332921 secondary_MeV 0.11421542414334142 entry_mm -0.22191046615496712 -0.10640180277229089 precursors 14997
library6 Edep_MeV 0.07025510896948331 secondary_MeV 0.04735201896948929 entry_mm -0.14571582708308711 0.0028721306909981759 precursors 7501
library7 Edep_MeV 0.061813631902973398 secondary_MeV 0.047336571902970963 entry_mm 0.22421562578882631 0.14158533293534126 precursors 6571
coverage 0 -0.22191046615496712 0.22421562578882631 -0.21783401707932315 0.21575798685676528 quadrants 3 1 2 2
holdout0 Edep_MeV 0.1211908886709599 secondary_MeV 0.09343346867099353 entry_mm -0.084515213205178574 0.001032265860637982 precursors 13065
holdout1 Edep_MeV 0.087657671580024035 secondary_MeV 0.065707391580054217 entry_mm -0.023174280506601919 -0.09407725994799615 precursors 9409
holdout2 Edep_MeV 0.082034696924966885 secondary_MeV 0.057601226924989674 entry_mm -0.22908890092545997 -0.069170844808377652 precursors 8799
holdout3 Edep_MeV 0.051604450705508342 secondary_MeV 0.039112280705505764 entry_mm -0.16631070314571 -0.10617634969088557 precursors 5553
holdout4 Edep_MeV 0.16210696462443772 secondary_MeV 0.13773946462442635 entry_mm 0.1780666140358978 -0.0046953088710011137 precursors 17411
holdout5 Edep_MeV 0.091080828882511275 secondary_MeV 0.066328678882539197 entry_mm 0.15407037985171196 0.049574370016194635 precursors 9837
holdout6 Edep_MeV 0.15210392065742231 secondary_MeV 0.12878296065742878 entry_mm -0.10865408284786854 0.13977558149495395 precursors 16368
holdout7 Edep_MeV 0.10860427202297358 secondary_MeV 0.084699432023000842 entry_mm -0.22839452438795693 0.035187968148541804 precursors 11658
coverage 1 -0.22908890092545997 0.1780666140358978 -0.10617634969088557 0.13977558149495395 quadrants 3 1 3 1
sample 0 mean_MeV 0.096619801156343318 SE_MeV 0.011165134384142853
sample 1 mean_MeV 0.1070479617586005 SE_MeV 0.013089360955960605
holdout_z -0.60613312786843376 massKg 1.2500000000000002e-07 rate_per_Gy_s 8074831.7112053391
chemistry H2O2^0 direct 248 import 227 difference -21 bound 140.26696830622021
chemistry H3O^1 direct 32853 import 32853 difference 0 bound 2852.1108972422435
chemistry HO_2°^0 direct 139 import 147 difference 8 bound 107.18920715172658
chemistry H^0 direct 6447 import 6337 difference -110 bound 934.07811320492328
chemistry H_2^0 direct 1813 import 1862 difference 49 bound 437.23066958946424
chemistry OH^-1 direct 577 import 568 difference -9 bound 225.9270917882636
chemistry O^-1 direct 76 import 75 difference -1 bound 76.749234364667046
chemistry O_2^0 direct 5 import 3 difference -2 bound 17.130562748477143
chemistry e_aq^-1 direct 32284 import 32294 difference 10 bound 2816.2921076175971
chemistry °OH^0 direct 39014 import 39185 difference 171 bound 3241.8250464807529
chemistry °O^0 direct 1125 import 1052 difference -73 bound 323.48999553491694
repeat H2O2^0 first 35 second 35
repeat H3O^1 first 5164 second 5164
repeat HO_2°^0 first 21 second 21
repeat H^0 first 1068 second 1068
repeat H_2^0 first 291 second 291
repeat OH^-1 first 97 second 97
repeat O^-1 first 18 second 18
repeat e_aq^-1 first 5060 second 5060
repeat °OH^0 first 6168 second 6168
repeat °O^0 first 190 second 190
VALIDATED
```

## Original diagnostic evidence (preserved)

The following is the original checkpoint report. Its “incomplete” status describes
that checkpoint, not the later implementation above.

# Precursor capture investigation — ticket 1 incomplete

The isolated MSVC RelWithDebInfo executable builds against configured Geant4
11.4.3. CTest `PrecursorBoundary` passed (8.93 s) for one 500 keV electron
in a 0.5 mm water cube. This is a central, parallel-entry diagnostic at
z = -249.999 um, not the required uniform-face library or a calibration.
Energy cuts are disabled after initialization. No production code is edited.

The StartProcessing hook observes delayed tracks before synchronization and
stops the scheduler. It observed 8,891 precursors: 4,118 e_aq^-1,
74 H2O^-1, 516 H2O^0 and 4,183 H2O^1. Every observed track has a 1 ps
activation time. Reconstructing a temporary molecule from the same in-process
configuration pointer preserved its decay-time value. The -1000 ps value
for solvated electrons is the recorded sentinel, not a physical negative age.

This is NOT an export/import demonstration. The TSV is diagnostic only: it
omits positions and electronic occupancies. In particular, configuration
names are definition plus charge (G4MolecularConfiguration.cc, lines 441–443),
so a name cannot distinguish all electronic states. Reusing configuration
pointers also cannot establish fresh-process reconstruction. A versioned
format must preserve full electronic occupancy/configuration identity,
status, position and timing and verify reconstruction in a fresh process.

Source boundary evidence: G4DNAChemistryManager.cc 631–662 creates modified
water with fStopButAlive at 1 ps; G4Scheduler.cc invokes StartProcessing before
delayed-list synchronization; G4DNAMolecularDissociation.cc 108–112 uses decay
time minus proper time and 258–260 creates alive products at parent global
time. The project also registers electron-hole recombination. Consequently
radical-only startup export is invalid.

Verification command (with Geant4 and vcpkg bin directories on PATH):

    ctest --test-dir .scratch/tests/2026-11-06__timed-track-injection-dose-rate-feasibility/01-msvc -L injection-packets --output-on-failure --no-tests=error

One diagnostic passed. Ticket acceptance has NOT passed. Remaining work:
full packet serialization and rejection tests, fresh-instance import and
chemical controls, uniform-face generation, all-secondary volume energy
scoring and abortion checks, independent library/holdout sampling, and
source-only Poisson/calibration tests. No chemistry equivalence, calibrated
rate, or feasibility conclusion is claimed. Ticket 2 must not begin.
