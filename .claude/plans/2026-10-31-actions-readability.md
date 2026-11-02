# Actions readability pass

Approved scope: the user's standing approval covers modifications, directory
commits, push and PR. Findings recorded before edits. Base: 1ec4f12.
Only src/actions, header/actions and directly related recipes belong here.

## Clarity

- ActionInitialization.cc:38: Build spans about 40 lines of operations plus
  commentary, mixing physical actions and chemistry setup. A possible
  ConfigureChemistryAction helper would hide the initialization sequence;
  retain the sequence here and remove stale experiment blocks at 51-70.
- Run.cc:62-66, StackingAction.cc:36,41-42 and TrackingAction.cc:46-52:
  remove commented-out debugging code. Keep the empty tracking override
  (public callback) rather than change the class interface.
- RunAction.hh:32: unused DoseAccumulable pointer/declaration. Flag as dead
  legacy state, defer removal to avoid an unnecessary class-layout change.
- Other callbacks are below roughly 50 executable lines; setup/merge ordering
  is explicit. Keep the original statements and all branches in place.

## Comments

- All eight .cc/.hh pairs: purpose, actual dependencies and thread expectations.
- ActionInitialization.cc:53-74: conditional defaults and serial Build timing
  matter; bulk material also contains the water/pH buffer, not only optional O2.
- ActionInitialization.cc:83-86: the claimed exact (nEvents+1)/2 bias is not
  universal with variable event yields/deposits. Explain the contamination risk.
- RunAction.cc:76: false disables RNG-state storage, contrary to the comment.
  Lines 96-103 also omit newer counters/table from the accumulation rationale.
- SteppingAction.hh:7-13: the G4DNA substring is the actual filter; Geant4 EM
  constructors also register standard EM processes without that substring.
- StackingAction.cc:56: Run processes chemistry synchronously. Preserve the
  existing later "Chemistry started" log string as an output contract.
- TrackingAction.cc:13: one primary is the gun default, not a restriction on
  /gun/number. Document the existing track-1 accounting convention.

## Naming

- Run.cc:24-75: mfdet/CollectionIDspecies/CollectionID/HCE/evtMap/itr/edep
  become multiFunctionalDetector/speciesCollectionId/collectionId/
  hitsCollections/eventEnergyDeposits/hit/energyDeposit_MeV.
- Run.hh:65: fSumEne -> fEnergyDeposit_MeV (private; Geant4 energy base is MeV).
  GetSumDose remains a public contract; document its energy, not mass-dose, result.
- ActionInitialization.cc:41 primGenAction; EventAction.cc:44-45 dir/err;
  RunAction.cc:88 nofEvents; StackingAction.hh:20 physList: descriptive names.
  Retain each file's member-prefix style and all Geant4 API/command/output names.

## Documentation

- All public action classes, constructors, callbacks, accessors and Beam:
  describe ownership/lifetime, input units, required Geant4 setup, failure
  behavior and master/worker responsibilities. No new dependencies.
- Related recipes add-a-per-run-counter, create-a-scorer,
  create-a-species-counter and mesoscopic-3d-species-counting: replace action
  line ranges with stable method references. Fix the species-reset bias claim
  and create-a-scorer's "one Run for the whole job" claim (one per beamOn).

## Source evidence

Geant4 11.4.1 source root as in the chemistry plan:
- source/run/src/G4RunManager.cc:974-977 calls Build on action installation;
  G4MTRunManager.cc:509 uses BuildForMaster; G4MTRunManagerKernel.cc creates
  worker actions. G4RunManagerKernel.cc:489-516 constructs particles when
  the physics list is installed, before project gun construction in Serial.
- source/run/src/G4MTRunManager.cc:580-583 locks MergeRun; its RunTermination
  waits for workers before the master EndOfRunAction.
- source/event/src/G4StackManager.cc:174-202 calls NewStage while urgent
  tracks are exhausted, after transferring waiting stacks.
- DNA utils/src/G4DNAChemistryManager.cc:304-334 runs scheduler Process
  synchronously; 778-803 forwards lifecycle callbacks to molecule counters.
- DNA utils/src/G4PhysChemIO.cc:50-51,94-101: FormattedText CloseFile skips
  uninitialized output; destroying its stream still closes it.
- DNA molecules/management/src/G4MoleculeCounterManager.cc:504-538 implements
  event/run resets; 653-654 initializes the reset flags false.
- G4MoleculeCounterTimeComparer.cc:89-115 treats times within precision as
  equivalent. source/event/src/G4ParticleGun.cc:203 onward validates the gun
  definition and generates its configured number of particles.
- G4EmDNAPhysics.cc:114-120 and G4EmDNAPhysics_option2.cc:79-86 both call
  ConstructStandardEmPhysics; G4EmDNABuilder.cc registers those standard
  processes, so substring coverage must not be claimed for every process.
- G4RunManager.cc:368,470 gates RNG-state files with storeRandomNumberStatus.

## Deferred issues and behavior risks

- GetSumDose is misleading; a public rename needs callers in scoring changed
  in a separate coordinated task. Existing filenames/manifest keys stay fixed.
- Run constructor uses a global collection ID as a primitive index and assumes
  successful detector/scorer lookup; already reported in scoring, not fixed.
- Multi-primary gun settings are not represented as multiple primary rows.
- Legacy unused DoseAccumulable state and the empty tracking callback remain.
- No logic, arithmetic, lifetimes, guards, order, logging strings, defaults or
  registration changes. Audit tokens after reversing local/private names.

## Verification required

Build Debug, run all ctest tests with debug HDF5 DLL path, build RelWithDebInfo,
then run the same 63-invocation/252-event SBS/IRT/mesoscopic matrix against the
original baseline. Record results before this concern's commit.

## Verification in progress

- Debug and RelWithDebInfo builds passed; ctest 17/17, 0.87 s, with the
  required debug DLL path. All 16 files pass the token audit after reversing
  the documented names. Release PE .text is byte-identical to pre-actions.
- The 63-invocation matrix is running. No simulation equivalence result is
  claimed until it completes; machine-code equality is independent evidence.

## Completed verification and directory deliverable

- Debug build and RelWithDebInfo build passed; ctest 17/17 passed in 0.87 s
  with vcpkg debug/bin first on PATH. No C++ edits after these builds.
- All 63 invocations/252 events passed: the three examples and 20 replicates
  per mode, 10 keV electrons, two beamOn 2 sub-runs per invocation.
- Against original baseline 1fcf8ba: matching output structures/time grids,
  229 screened scalar observables, 3309 descriptive curve points, zero
  Holm-adjusted flags at alpha 0.01. Minimum raw p = 0.07082748; adjusted = 1.
  Only the known WrongResolution warning occurred.
- Exact TrackLengths.csv and PhysicsInteractions.csv matches: each 10/42 SBS,
  18/42 IRT and 23/42 mesoscopic. Exact equality is not required.
- All 16 source/header token audits pass after reversing local/private names;
  release PE .text is byte-identical to pre-actions. git diff --check passes.
- Evidence: .scratch/readability-2026-10-29/actions/{comparison.json,
  statistics.csv,curves.csv,matrix-status.json}, actions-ctest.log,
  actions-token-audit.json and actions-code-sections.json.

Files: all eight action .cc/.hh pairs, the four related recipes listed above,
and this plan. Categories: descriptive local/private names and units,
source-checked lifecycle rationale, API contracts, stale-comment removal and
recipe corrections. No executable operations changed beyond identifier names.
Statistical non-detection supports preservation; it is not proof of equality.
Deferred issues above remain unchanged, including GetSumDose and unused state.
