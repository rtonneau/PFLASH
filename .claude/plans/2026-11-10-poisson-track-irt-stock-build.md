# poisson-track-irt stock-Geant4 build Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** `examples/poisson-track-irt` builds, runs and passes ctest on a stock Geant4 11.4.3 install. The patched build stays selectable, and every approximation the stock build makes is measured, reported in the ledger and documented.

**Architecture:** A CMake option `PTIRT_PATCHED_GEANT4` (default OFF) selects the build mode. A configure-time compile check confirms that the option matches the Geant4 headers. A Geant4-free `BuildMode.hh` makes the mode visible to the code, the compatibility tag, `capture.json` and `ledger.json`. The stock build makes up for the missing source-contact patch in two ways:
- capture resolves the startup contacts before writing a packet;
- a brute-force new-old contact diagnostic measures the contacts the stock kernel misses at each activation.

New logic (fold, coincidence finder, contact counter, box guard, build comparison) lives in Geant4-free core files with unit tests.

**Tech Stack:** C++20, Geant4 11.4.3 (stock at `${G4_ROOT}`; patched install under `.scratch/tests/2026-11-08__beam-on-irt-beam-off-meso/install`), CMake 3.16+ with Ninja, MSVC x64, CTest, PowerShell 7 for the validation scripts.

**Spec:** the user's feature request of 2026-11-10 (session transcript `145ad179-…`), summarised verbatim in the "Spec digest" below. The plan argues from that digest.

### Spec digest
1. CMake option `PTIRT_PATCHED_GEANT4` (default OFF). ON keeps `ResetSourceActivations`/`RegisterSourceActivation`. A mismatch is caught at configure time (try_compile against `G4DNAIndependentReactionTimeStepper.hh`), or the build fails clearly. The mode is recorded in capture.json, ledger.json and the compatibility tag.
2. Box fold: both builds use the example's own correct fold in `HistoryBoundedBrownianAction`. Document that in-box transport in the stock build still uses the stock kernel fold. Guard or warn when `halfBoxUm` is not large compared with sqrt(6 D t_end) for the fastest species, and report it in the ledger. Do not copy `G4ChemReboundTransportation`.
3. Coincidence guard in `SourceInjector` (before any Geant4 state changes) and in capture: fail with a named error that reports the count. Do not fix it silently.
4. Capture-resolved contacts: (a) spike first, **STOP if it fails**; (b) capture after the startup contact step, with a new age and tag so old libraries are refused, and a reproducible chemistry random state; (c) measure the first-packet double trial, then decide and record the decision.
5. Missed-contact diagnostic per activation and per species pair in ledger.json, with an optional failure threshold. Brute force is acceptable if its cost is measured.
6. Validation: ctest in both builds, the separated check in the stock build, a stock-vs-patched comparison (same seeds, deterministic schedule, existing margins, clustered by physical packet, with contact counts) and an overlap sweep.
7. Reporting: ADR 0010, README sections, UHDR correction, a re-enabling checklist, upstream notes (prepared, not submitted), raw evidence in `.scratch/`. **Every check that was not run is reported as not run.**

## Global Constraints

- Geant4 11.4.3 only; `find_package(Geant4 11.4.3 REQUIRED)` stays.
- Do not change Geant4 sources or the shared install `${G4_ROOT}`.
- Do not subclass or copy kernel classes (`G4ChemReboundTransportation`, the stepper, the finder).
- Keep `patches/geant4/11.4.3/*` and their READMEs byte-identical; link to them.
- Out of scope: the mesoscopic stage, O2/scavengers, pulse structure, dose-rate claims, and changes to the main PFLASH app (`src/`, `header/`, `macro/` at the repo root). Exception: the doc corrections in task 10.
- C++ follows Geant4 style: PascalCase classes and files, camelCase variables. Match the surrounding file.
- Never hardcode Geant4 paths. Scripts read `Geant4_DIR` from each build's `CMakeCache.txt`.
- Build and test following `.claude/geant4-instructions.md` (MSVC environment, Ninja, absolute paths).
- Exploratory equivalence margin: `max(0.05 × patched mean, 1 molecule)`, clustered by physical packet with t CI95 (df = K−1), no familywise claim.
- Separated-check tolerance stays `|C−E| <= 4·sqrt(SE_C²+SE_E²) + 0.05·E` (`scripts/RunSeparatedCheck.ps1`).
- Raw evidence goes in `.scratch/tests/2026-11-10__poisson-track-irt-stock-build/`. Tracked docs summarise it and give paths.
- Work on branch `feat/poisson-track-irt-stock-build` (create it from `main`). Commit at the end of each task. Commit messages end with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- Anything not run (for example the patched ctest, if the patched install cannot be loaded) is written as **NOT RUN**, with the reason, in the evidence README, the ADR and the final report.

## Review Focus

1. **Coincident products of startup reactions.** `G4DNAMakeReaction` places all products of a reaction at one point, so a post-startup packet can legitimately contain coincident molecules. The strict guard would then refuse ordinary packets. The spike's gate G2 measures this before the guard is written. Test: `CaptureAfterStartupContacts` asserts `coincidentMolecules == 0` across 16 seeds, or the plan stops at G2.
2. **A library from the other build or an old library.** A user points a stock history at a patched or `ptirt1_` library and expects a clear refusal, not a silently wrong history. Tests: `HistoryRejectsIncompatibleTag` (ptirt1), `HistoryRejectsPatchedLibrary` (stock) and `HistoryRejectsStockLibrary` (patched).
3. **The option and the install disagree.** A user configures `-DPTIRT_PATCHED_GEANT4=ON` against the stock install, or OFF against a patched one, and expects configure to fail with a message naming both. Test: a manual configure check in task 2, recorded in the evidence.
4. **A small box with a long endpoint.** A user sets `halfBoxUm` close to the diffusion length and expects a warning plus a ledger status, not a silent kernel-fold artefact. Test: unit test `BoxGuardTest` (warning below ratio 3), plus the `HistoryReflectingBoxSmoke` ledger assertion `boxFoldGuard.status == "ok"`.
5. **Contact threshold exceeded.** A user sets `newOldContactLimit 0` on a schedule with contacts and expects exit 1, a written ledger with `allPassed: false`, and `PTIRT_NEW_OLD_CONTACTS` on stderr. Test: `HistoryNewOldContactLimit`.

---

## File Structure

New, Geant4-free (in `ptirtCore`, unit-tested):
- `include/BuildMode.hh`: `kPatchedGeant4`, `kBuildModeName` ("stock"/"patched"), `kCaptureStageName`, `kKernelFoldName`. `#error` if `PTIRT_PATCHED_GEANT4` is undefined.
- `include/ReflectingFold.hh` + `src/ReflectingFold.cc`: the example's own fold.
- `include/CoincidenceCheck.hh` + `src/CoincidenceCheck.cc`: exact-position coincidence finder.
- `include/ContactCount.hh` + `src/ContactCount.cc`: brute-force within-radius pair counts.
- `include/BoxGuard.hh` + `src/BoxGuard.cc`: diffusion-length ratio and status.
- `include/BuildComparison.hh` + `src/BuildComparison.cc`: clustered paired comparison.
- `include/PacketDigest.hh` + `src/PacketDigest.cc`: FNV-1a 64 digest of a molecule list.

New, Geant4/OS-facing:
- `include/LoadedModules.hh` + `src/LoadedModules.cc`: lists the loaded G4 DLL paths (Windows psapi; elsewhere an empty list marked "unsupported").
- `src/CompareBuildsMain.cc`: the `compare-builds` executable.
- `scripts/RunBuildComparison.ps1`, `scripts/RunOverlapSweep.ps1`.
- `test/*Test.cc` for each core file, plus `test/RunCaptureStageCheck.cmake`, `test/RunModuleCheck.cmake`, `test/RunHistoryContactLimit.cmake`.
- `docs/adr/0010-poisson-track-irt-stock-build.md` (repo root `docs/adr/`), `examples/poisson-track-irt/docs/re-enabling-patch.md`, `examples/poisson-track-irt/docs/upstream-notes.md`.

Modified: `CMakeLists.txt`, `include/ExamplePhysicsTag.hh`, `src/SourceInjector.cc`, `src/HistoryBoundedBrownianAction.cc` (+ `.hh` comment), `include/ExamplePhysicsList.hh` (comment), `src/CaptureRecorder.cc`/`.hh`, `src/CaptureActions.cc`/`.hh`, `src/ActivationObserver.cc`/`.hh`, `src/HistoryMain.cc`, `include/HistoryConfig.hh`/`src`, `README.md`, `docs/dose-rate/mesoscopic-feasibility.md:25`.

All paths below are relative to `examples/poisson-track-irt/` unless they start with `docs/adr/` or `.scratch/`.

---

### Task 1: Spike: startup contacts on stock 11.4.3 (gates G1, G2, G3)

Throwaway. Nothing in this task is committed except the evidence README.

**Files:**
- Create: `.scratch/tests/2026-11-10__poisson-track-irt-stock-build/spike/` (a copy of the example, with `SourceInjector.cc` patch calls stubbed out by `#if 0`; only the `capture` target is built against `${G4_ROOT}`)
- Create: `.scratch/tests/2026-11-10__poisson-track-irt-stock-build/spike/README.md` (evidence)

- [ ] **Step 1:** Copy the example into the spike directory. Add a `SpikeTimeStepAction` that, in `UserPreTimeStepAction`/`UserPostTimeStepAction`, logs per step: `GetGlobalTime()`, `GetTimeStep()`, `IsInteractionStep()`, the species counts by `G4TrackStatus`, and every reaction (`UserReactionAction`: reactants, products, product positions). At the first step with `GetTimeStep() > 0`, before it runs, it logs: the exact-coincidence count among `fAlive` main-list molecules; every alive pair with d² < Reff² (species pair, d, Reff, `GetProbability()`); and Σp. It then calls `G4Scheduler::Instance()->Stop()`.
- [ ] **Step 2:** Configure and build `capture` against `${G4_ROOT}` in `spike/build`. Run 16 packets (`--first-seed 1001 --count 16`) twice. Save the logs.
- [ ] **Step 3: Evaluate gate G1.** It passes only if all of these hold for every packet:
  - one or more zero-time steps at 1 ps carry startup reactions;
  - the first positive step is reached;
  - `Stop()` returns with no `G4Exception` and an exit code of 0;
  - the two runs give byte-identical logs.

  **If G1 fails: STOP, write the evidence README and report to the user. Do not start task 2.**
- [ ] **Step 4: Evaluate gate G2.** Count the coincident molecules at the capture point over the 16 packets. If any are nonzero: **STOP and report to the user.** The strict capture guard of task 4 would refuse ordinary packets, and the user must choose how to proceed.
- [ ] **Step 5: Record G3.** Per packet, record the number of within-Reff pairs and Σp at the capture point, with the mean and max over 16 packets. Use it in task 6.
- [ ] **Step 6:** Write the evidence README (commands, the Geant4 DLL paths loaded, the gate verdicts, the numbers). Commit it alone: `git add .scratch/tests/2026-11-10__poisson-track-irt-stock-build/spike/README.md` (only if `.scratch` is tracked here; otherwise leave it untracked and say so).

---

### Task 2: Build mode: CMake option, configure check, BuildMode.hh, tag, module listing

**Files:**
- Modify: `CMakeLists.txt` (after line 14, `find_package`)
- Create: `include/BuildMode.hh`, `include/LoadedModules.hh`, `src/LoadedModules.cc`
- Modify: `include/ExamplePhysicsTag.hh`, `src/SourceInjector.cc:195,211`
- Test: `test/BuildModeTest.cc`

**Interfaces:**
- Produces: `ptirt::kPatchedGeant4` (constexpr bool), `ptirt::kBuildModeName` (`"stock"`|`"patched"`), `ptirt::kCaptureStageName` (`"afterStartupContacts"` in stock, `"chemistryStart"` in patched), `ptirt::kKernelFoldName` (`"stock"`|`"patched"`).
- Produces: `std::vector<std::string> ListLoadedGeant4Modules();` (full paths of loaded modules whose file name starts with `G4`, case-insensitive) and `bool LoadedModulesSupported();`.
- Produces: `kPacketCompatibilityTag` =
  `"ptirt2_geant4-11.4.3-" + kBuildModeName + "_G4EmDNAPhysics_option2_G4EmDNAChemistry_option3_IRT_syn_pureWater_age1ps-" + kCaptureStageName`, as a constexpr string chosen by `#if`.

- [ ] **Step 1:** Write `BuildModeTest`. It asserts that the tag starts with `ptirt2_`, that it contains `kBuildModeName` and `kCaptureStageName`, and that stock ↔ `afterStartupContacts` and patched ↔ `chemistryStart`.
- [ ] **Step 2:** Add `option(PTIRT_PATCHED_GEANT4 "Build against a Geant4 with the timed-injection patches" OFF)`. Then run `check_cxx_source_compiles` with `CMAKE_REQUIRED_INCLUDES=${Geant4_INCLUDE_DIRS}`, `CMAKE_REQUIRED_DEFINITIONS` from Geant4 and a source that calls `&G4DNAIndependentReactionTimeStepper::ResetSourceActivations`, giving `PTIRT_GEANT4_HAS_SOURCE_PATCH`. Use `message(FATAL_ERROR …)` when ON without the API ("PTIRT_PATCHED_GEANT4=ON but <Geant4_DIR> has no source-activation API; see patches/geant4/11.4.3/README.md") and when OFF with the API ("this Geant4 is patched; configure with -DPTIRT_PATCHED_GEANT4=ON"). Add `target_compile_definitions(ptirtCore PUBLIC PTIRT_PATCHED_GEANT4=$<BOOL:…>)` (as 0/1). On WIN32, link `psapi` into the executables that use `LoadedModules`.
- [ ] **Step 3:** Create `BuildMode.hh` and the tag. Guard the two `SourceInjector` calls with `#if PTIRT_PATCHED_GEANT4`.
- [ ] **Step 4:** Implement `LoadedModules.cc` (`EnumProcessModules` + `GetModuleFileNameExW`, converted to UTF-8).
- [ ] **Step 5:** Configure the stock build in `build-stock/` and build it. Run `ctest -R BuildMode`; expect PASS. Then run the mismatch configure (`-DPTIRT_PATCHED_GEANT4=ON` against `${G4_ROOT}`) in a throwaway directory, expect FATAL_ERROR, and save its output to evidence `configure-mismatch.txt`. Run the patched configure against the patched install in `build-patched/`; expect success. If that install is missing or unusable, record **NOT RUN** with the reason.
- [ ] **Step 6:** Commit with `feat(ptirt): select stock or patched Geant4 build mode`.

---

### Task 3: Own reflecting fold and box guard

**Files:**
- Create: `include/ReflectingFold.hh`, `src/ReflectingFold.cc`, `include/BoxGuard.hh`, `src/BoxGuard.cc`
- Modify: `src/HistoryBoundedBrownianAction.cc` (`Fold()`), `include/HistoryBoundedBrownianAction.hh:4-12`, `include/ExamplePhysicsList.hh:17-23` (comments)
- Test: `test/ReflectingFoldTest.cc`, `test/BoxGuardTest.cc`

**Interfaces:**
- Produces: `double ReflectIntoInterval(double x, double low, double high);`. It throws `std::invalid_argument` when an input is non-finite or `high <= low`. Algorithm: `L = high-low; f = fmod(x-low, 2L); if (f<0) f += 2L; return low + (f <= L ? f : 2L - f);`.
- Produces: `struct BoxGuardResult { std::string status; double ratio; double diffusionLengthNm; double halfBoxNm; std::string fastestSpecies; double fastestDiffusionNm2PerNs; double endpointNs; };`, `constexpr double kBoxGuardMinRatio = 3.0;` and `BoxGuardResult EvaluateBoxGuard(double halfBoxNm, double endpointNs, const std::string& fastestSpecies, double dNm2PerNs);`. Status is `"none"` when `halfBoxNm <= 0`, `"ok"` when ratio ≥ 3 and `"warning"` otherwise. The diffusion length is `sqrt(6·D·t)`.

- [ ] **Step 1:** Write `ReflectingFoldTest` on [0,1]: 2.1→0.1, 2.9→0.9, 1.0→1.0, −0.3→0.3, 1.3→0.7, −2.5→0.5, −1.0→1.0, 0.5→0.5; on [−5,−3]: −2.5→−3.5. It also checks that NaN and `high <= low` throw. Write `BoxGuardTest`: halfBox 0 → none; ratio exactly 3 → ok; 2.99 → warning; and the diffusion length for D = 4.9 nm²/ns (e_aq, 4.9e-9 m²/s; the code uses the value passed in, never a constant) at t = 1e6 ns equals `sqrt(6·4.9·1e6)` ≈ 5422 nm to 1e-12 relative.
- [ ] **Step 2:** Run them and verify they fail (no symbols yet).
- [ ] **Step 3:** Implement both. Replace `G4ChemReboundTransportation::calculateNextCoordinate` in `Fold()` with `ReflectIntoInterval` per axis, and drop that include.
- [ ] **Step 4:** In `HistoryMain`, after `Initialize`, when `halfBoxUm > 0`, find the fastest species by iterating `G4MoleculeTable::Instance()->GetConfigurationIterator()` (max `GetDiffusionCoefficient()`). Then call `EvaluateBoxGuard(halfBoxUm·1000, endpointNs, …)`. On `"warning"` in the stock build, print `PTIRT_BOX_GUARD_WARNING ratio=<r> species=<s>` to stderr and continue. In the patched build the warning reads `PTIRT_BOX_GUARD_NOTE` (the kernel fold is patched there). Store the result for the ledger (task 7 writes it).
- [ ] **Step 5:** Rewrite the comments in `HistoryBoundedBrownianAction.hh` and `ExamplePhysicsList.hh` to say what is true: the example's own fold applies at history boundaries in both builds; in-box transport in the stock build still goes through `G4ChemReboundTransportation`, which is wrong for overshoots ≥ 2L and at n = 1 exactly; UHDR also uses that kernel class (`EmDNAChemistry.cc:352`).
- [ ] **Step 6:** Run `ctest -R "ReflectingFold|BoxGuard|ReflectingBox"` in `build-stock`; expect PASS. Commit with `feat(ptirt): own reflecting fold and box-size guard`.

---

### Task 4: Coincidence guard

**Files:**
- Create: `include/CoincidenceCheck.hh`, `src/CoincidenceCheck.cc`
- Modify: `src/SourceInjector.cc` (constructor validation, before any `G4ITTrackHolder`/stepper call), `src/CaptureRecorder.cc` (before writing)
- Test: `test/CoincidenceCheckTest.cc`, plus a ctest `HistoryRejectsCoincidentPacket` (a hand-made library whose packet has two molecules at the same position, written by a small CMake script like the existing refusal fixtures)

**Interfaces:**
- Produces: `struct CoincidenceReport { std::size_t coincidentMolecules; std::size_t positions; };` and `CoincidenceReport FindCoincidentPositions(const std::vector<std::array<double,3>>& positionsNm);`. It sorts and compares exactly with `==`. It counts every molecule that shares a position with another, and the number of distinct shared positions.
- Produces: the error token `PTIRT_COINCIDENT_MOLECULES`.

- [ ] **Step 1:** Write the unit test: an empty list → {0,0}; three distinct → {0,0}; two equal plus one distinct → {2,1}; three equal plus two equal → {5,2}; values that differ by 1 ulp are not coincident.
- [ ] **Step 2:** Run it and verify that it fails.
- [ ] **Step 3:** Implement it. Coincidences are checked within each packet and across the whole injected set (positions plus packet origin offsets). In `SourceInjector`, throw `std::runtime_error("PTIRT_COINCIDENT_MOLECULES count=<n> positions=<p>")` from the constructor; `HistoryMain` already maps constructor exceptions to exit 2. Record `injected.coincidentMolecules` (always 0 on success) for the ledger. In capture, call `Fail("PTIRT_COINCIDENT_MOLECULES …")` before anything is written, and record `coincidentMolecules: 0` per packet in capture.json.
- [ ] **Step 4:** Add `HistoryRejectsCoincidentPacket` through the existing `ptirt_history_refusal` function (CMakeLists.txt:234), expecting exit 2 and the token. Run `ctest -R Coincident`; expect PASS. Commit with `feat(ptirt): refuse coincident molecules at capture and injection`.

---

### Task 5: Stock capture after the startup contacts, with chemistry seed and digest

**Files:**
- Modify: `include/CaptureActions.hh`, `src/CaptureActions.cc` (`CaptureTimeStepAction`), `include/CaptureRecorder.hh`, `src/CaptureRecorder.cc`
- Create: `include/PacketDigest.hh`, `src/PacketDigest.cc`
- Test: `test/PacketDigestTest.cc`, `test/RunCaptureStageCheck.cmake` (ctest `CaptureAfterStartupContacts`, stock only); `test/RunCaptureSeedCheck.cmake` stays valid unchanged

**Interfaces:**
- Consumes: `kCaptureStageName`, `kPatchedGeant4` (task 2), `FindCoincidentPositions` (task 4).
- Produces: `std::uint64_t DigestMolecules(const std::vector<PacketMolecule>&);`. This is FNV-1a 64 over the species name, the three position doubles as bytes, and the time, in list order after sorting by (species, x, y, z).
- Produces: `long ChemistrySeedFor(long physicsSeed)` = `kMaxHistorySeed + 1 - physicsSeed`. `kMaxPhysicsSeed = 1073741823`; capture refuses larger physics seeds with exit 2.
- Produces: capture.json per-packet fields `captureStage`, `chemistrySeed` (stock only), `chemistryStartDigest` (hex, both builds), `capturedDigest` (hex), `coincidentMolecules`, `startupReactions` (stock only), `startupZeroSteps` (stock only), `secondTrial {pairs, expectedReactions}` (stock only, task 6). Top-level fields: `buildMode`, `loadedGeant4Modules`.

- [ ] **Step 1:** Write `PacketDigestTest`: the digest is order-independent (two input orders give the same value), and a 1-ulp change in x or a renamed species changes it. Write `RunCaptureStageCheck.cmake`. It runs `capture --first-seed 1001 --count 4` twice and asserts that the library SHA256s are identical, every packet has `captureStage == "afterStartupContacts"`, `startupZeroSteps >= 1`, `coincidentMolecules == 0`, and `chemistryStartDigest != capturedDigest` for at least one packet. Every packet time must be 1 ps.
- [ ] **Step 2:** Run them and verify that they fail.
- [ ] **Step 3:** Implement it:
  - At `StartProcessing`, digest the delayed lists as today (the `chemistryStartDigest`, in both builds).
  - Patched build: capture there and `Stop()`, as today.
  - Stock build: reseed `G4Random` with `ChemistrySeedFor(physicsSeed)` at `StartProcessing`. In `UserPreTimeStepAction`, at the first `GetTimeStep() > 0`, copy the main-list `fAlive` tracks (all at 1 ps; fail with a named error, `PTIRT_CAPTURE_STATE`, if any delayed or `fStopButAlive` track remains), run the coincidence check, then call `Stop()`. Count zero-time steps and startup reactions (a `UserReactionAction` counter).
- [ ] **Step 4:** Run `ctest -R "PacketDigest|CaptureAfterStartupContacts|CaptureSeedReproducible|CaptureSmoke"` in `build-stock`; expect PASS. Commit with `feat(ptirt): stock capture resolves startup contacts`.

---

### Task 6: First-packet double trial: measure and decide

**Files:**
- Modify: `src/CaptureRecorder.cc` (fill `secondTrial` at the capture point using `CountInternalContacts` from task 7; this task is ordered after task 7's core file, see note)
- Create: `.scratch/tests/2026-11-10__poisson-track-irt-stock-build/double-trial/README.md`

Note: implement `ContactCount.{hh,cc}` (task 7, Steps 1–3) before this task. The two tasks share it.

**Interfaces:**
- Consumes: `CountInternalContacts`, `ContactRadiusFn` (task 7).
- Produces: the ledger field `firstPacketSecondTrial {pairs, expectedReactions, decision}` (stock only; `decision` is `"accepted"`). The history copies it from the library entry of the packet at activation index 0.

- [ ] **Step 1:** At the stock capture point, compute the within-Reff alive pairs and Σ `GetProbability()` (expected reactions from the second trial that the kernel will run when this packet is the first source of a history). Write them to capture.json as `secondTrial`.
- [ ] **Step 2:** Capture 64 packets (`--first-seed 3001 --count 64`). Report the mean, max and distribution of `expectedReactions` in the evidence README.
- [ ] **Step 3: Decision rule.** If the mean ≤ 1 expected extra reaction per first packet, record **accept and report** in the evidence and in ADR 0010. Task 9 confirms it against the stock-vs-patched comparison. Otherwise **STOP and report to the user**; a sentinel-start mitigation would need separate investigation and is not part of this plan.
- [ ] **Step 4:** Commit with `feat(ptirt): measure first-packet second contact trial`.

---

### Task 7: New-old contact diagnostic and threshold; ledger fields

**Files:**
- Create: `include/ContactCount.hh`, `src/ContactCount.cc`
- Modify: `include/ActivationObserver.hh`, `src/ActivationObserver.cc`, `src/HistoryMain.cc` (`LedgerInput`, `MakeLedger`, the `Run()` exit path), `include/HistoryConfig.hh` + its parser
- Test: `test/ContactCountTest.cc`, `test/RunHistoryContactLimit.cmake` (ctest `HistoryNewOldContactLimit`, stock only), `test/HistoryConfigTest.cc` (new key)

**Interfaces:**
- Produces:
  ```cpp
  struct ContactMolecule { std::string species; std::array<double,3> positionNm; };
  using ContactRadiusFn = std::function<double(const std::string&, const std::string&)>; // < 0: no reaction
  struct ContactCounts { std::size_t pairs; std::size_t newMoleculesInContact;
                         std::map<std::string, std::size_t> perPair; double cpuSeconds; };
  ContactCounts CountNewOldContacts(const std::vector<ContactMolecule>& fresh,
                                    const std::vector<ContactMolecule>& old, const ContactRadiusFn&);
  ContactCounts CountInternalContacts(const std::vector<ContactMolecule>&, const ContactRadiusFn&);
  std::string PairLabel(const std::string& a, const std::string& b); // "A + B", sorted by name
  ```
  The contact condition is strict, d² < R². `cpuSeconds` comes from `std::chrono::steady_clock` around the loop.
- Produces: the config key `newOldContactLimit` (optional integer ≥ 0; absent = no limit; a negative or non-integer value is a parse error, exit 2).
- Produces the ledger fields (formatVersion 2):
  - `buildMode`, `captureStage`, `kernelFold`;
  - `boxFoldGuard {status, ratio, diffusionLengthNm, halfBoxNm, fastestSpecies, minRatio}`;
  - `newOldContacts {pairs, newMoleculesInContact, perPair{}, perActivation[{index, timeNs, pairs}], limit|null, trialledByKernel, cpuSeconds}` (`trialledByKernel`: false in stock, true in patched);
  - `firstPacketSecondTrial` (task 6);
  - `injected.coincidentMolecules`;
  - `loadedGeant4Modules`.

- [ ] **Step 1:** Write `ContactCountTest` with a radius function that gives 1.0 for A+B, −1 otherwise:
  - fresh A at 0 and old B at 0.999 → 1 pair, perPair {"A + B": 1};
  - at 1.0 → 0 pairs (strict);
  - A+A pairs → 0;
  - two fresh molecules near one old one → pairs 2, newMoleculesInContact 2;
  - internal contacts count each pair once;
  - `PairLabel("B","A") == "A + B"`.
- [ ] **Step 2:** Run it and verify that it fails. Then implement it and run it again; expect PASS.
- [ ] **Step 3:** In `ActivationObserver`, at each activation (the existing hook where fresh tracks become active), build the fresh list and the old alive list from `G4ITTrackHolder` and call `CountNewOldContacts`. The radius comes from `G4DNAMolecularReactionTable::GetReactionTable()->GetReactionData(confA, confB)->GetEffectiveReactionRadius()`, or −1 when there is no data. Accumulate per activation. In the patched build, label the counts informational (they are taken at NewStage, before dissociation; see the ADR).
- [ ] **Step 4:** Write `RunHistoryContactLimit.cmake`. It builds a two-source schedule, packets 0 and 1 with offset 0.005 um at 10 ns, endpoint 1 us, with `newOldContactLimit 0`. It asserts exit 1, `PTIRT_NEW_OLD_CONTACTS` on stderr, and that ledger.json exists with `checks.allPassed == false` and `newOldContacts.pairs > 0`. Extend the existing smoke ledger asserts (`HistorySmoke`, `HistoryReflectingBoxSmoke`) to require `buildMode`, `newOldContacts`, `boxFoldGuard.status` (`"none"`/`"ok"`) and `injected.coincidentMolecules == 0`.
- [ ] **Step 5:** Implement the ledger fields and the limit path. When the limit is exceeded, write species.csv, activations.csv and ledger.json with `allPassed: false`, print `HISTORY_ERROR PTIRT_NEW_OLD_CONTACTS pairs=<n> limit=<l>`, and exit 1. Every other failure path is unchanged.
- [ ] **Step 6:** Run the full `ctest` in `build-stock`; expect all PASS. Commit with `feat(ptirt): new-old contact diagnostic and ledger build fields`.

---

### Task 8: Mode-specific ctests and the patched-build pass

**Files:**
- Modify: `CMakeLists.txt` (test registration under `if(PTIRT_PATCHED_GEANT4)` / `else()`)
- Create: `test/RunModuleCheck.cmake`

**Interfaces:**
- Consumes: the tests from tasks 2–7; `ListLoadedGeant4Modules` through the `capture.json` and `ledger.json` field `loadedGeant4Modules`.

- [ ] **Step 1:** Register the tests:
  - stock: `CaptureAfterStartupContacts`, `StockBuildModuleCheck`, `HistoryRejectsPatchedLibrary`, `HistoryNewOldContactLimit`;
  - patched: `HistoryRejectsStockLibrary`;
  - both: the unit tests, `HistoryRejectsCoincidentPacket`, and `HistoryRejectsIncompatibleTag` (now fed a `ptirt1_` tag).

  The other-build library fixture is a copy of the fixture library with its tag line rewritten (CMake `file(READ/WRITE)`).
- [ ] **Step 2:** In `RunModuleCheck.cmake`, run `history` on the smoke config, then assert that every path in `ledger.json`'s `loadedGeant4Modules` lies under `${Geant4_DIR}/../../../bin` (normalised, case-insensitive) and that the list is non-empty.
- [ ] **Step 3:** Run the full `ctest` in `build-stock`; record the count and duration in the evidence. Run the full `ctest` in `build-patched` with the patched install on PATH. If that is impossible, record **NOT RUN** with the exact reason.
- [ ] **Step 4:** Commit with `test(ptirt): mode-specific ctests`.

---

### Task 9: Build comparator, validation runs and evidence

**Files:**
- Create: `include/BuildComparison.hh`, `src/BuildComparison.cc`, `src/CompareBuildsMain.cc`, `test/BuildComparisonTest.cc`
- Create: `scripts/RunBuildComparison.ps1`, `scripts/RunOverlapSweep.ps1`
- Create: evidence under `.scratch/tests/2026-11-10__poisson-track-irt-stock-build/{comparison,overlap,separated}/`

**Interfaces:**
- Produces:
  ```cpp
  struct ClusterDiff { double timeNs; std::string species; std::vector<double> clusterMeanDiffs; double patchedMean; };
  struct ComparisonRow { double timeNs; std::string species; double meanDiff, ciLow, ciHigh, margin;
                         std::string verdict; bool biasFlag; };
  double StudentT95(int df);   // two-sided 95 %, table for df 1..30, throws outside
  ComparisonRow Compare(const ClusterDiff&);
  ```
  The margin is `max(0.05 × patchedMean, 1)`. Verdicts: `EQUIVALENT` (CI ⊂ [−m, m]), `DIFFERENT` (CI ∩ [−m, m] = ∅), `INCONCLUSIVE`, `EMPTY` (both means 0). `biasFlag` is set when the CI excludes 0.
- Produces: `compare-builds --stock <dir> --patched <dir> --out <csv>`. It reads the per-history species.csv in both trees, pairs them by (cluster, seed), and exits 0 (report only).

- [ ] **Step 1:** Write `BuildComparisonTest`: `StudentT95(7) == 2.364624` (to 1e-6) and `StudentT95(0)` throws; diffs {0,0,0,0} with patched mean 100 → EQUIVALENT, no bias; diffs all 10 with mean 100 → DIFFERENT, bias; a wide spread → INCONCLUSIVE; mean 0 with diffs 0 → EMPTY.
- [ ] **Step 2:** Run it to verify that it fails, implement it, then run it again; expect PASS. Add the `compare-builds` target.
- [ ] **Step 3:** `RunBuildComparison.ps1`:
  - for each build, capture 8 cluster libraries of 3 packets (first seeds 2001, 2101, …, 2701);
  - assert that the `chemistryStartDigest` values match between the builds for every packet, and abort if not;
  - per cluster, run 16 histories (`chemistrySeed = 10000 + 100·k + s`) with `config/deterministic.cfg`, endpoint 3 ms;
  - run `compare-builds`, then summarise the verdict counts, the bias flags and the per-pair `newOldContacts` totals per build.

  Run it and save the outputs and a README.
- [ ] **Step 4:** `RunOverlapSweep.ps1`: two-source schedules (packet 0 at t = 0, origin 0; packet 1 at 10 ns, offset d·x̂), with d ∈ {0.005, 0.05, 0.5, 5, 100} um, endpoint 1 us, 8 clusters × 8 seeds, in both builds. Per d, report the stock `newOldContacts.pairs` (mean ± SE) and the comparator verdicts. Expected result: the contacts rise as d falls, and the builds diverge at small d. Record what is actually seen, including a null result.
- [ ] **Step 5:** Run `scripts/RunSeparatedCheck.ps1` against `build-stock` (16 combined + 16×3 single, 3 ms, 97 record times). Save the PASS/FAIL count.
- [ ] **Step 6:** If the patched build could not run, steps 3–4 are **NOT RUN** for the comparison part; still run the stock half and report the contacts. Commit the code and scripts with `feat(ptirt): stock vs patched comparison and overlap sweep`.

---

### Task 10: ADR 0010, README, UHDR correction, checklist, upstream notes

**Files:**
- Create: `docs/adr/0010-poisson-track-irt-stock-build.md`, `examples/poisson-track-irt/docs/re-enabling-patch.md`, `examples/poisson-track-irt/docs/upstream-notes.md`
- Modify: `examples/poisson-track-irt/README.md` (Dependencies at lines 41–53, Build and test, Known limits around 300–325; keep the patch table and recipe at lines 55–90 as they are), `docs/dose-rate/mesoscopic-feasibility.md:25`

- [ ] **Step 1: ADR 0010.**
  - Context.
  - What each of the three patches fixes (source contacts, molecular finder, rebound fold), with links to `patches/geant4/11.4.3/*/README.md`.
  - What the stock build approximates, each with its measured size from tasks 1, 6 and 9:
    - startup contacts are resolved at capture;
    - the first-packet second trial;
    - missed new-old contacts;
    - the in-box kernel fold;
    - coincident distance-zero partners (guarded).
  - The conditions that require the patched build: a nonzero new-old contact count that matters, overlap at small d, a box-guard warning, or coincidences.
  - The decision on the double trial.
  - Rejected options: subclassing or copying kernel classes (the octree finder singleton is split between exe and DLL; thread-local accessors fail on Windows), and a sentinel start (not investigated).
  - Consequences.
  - The not-run list.
- [ ] **Step 2: README.** Sections:
  - Build mode (`PTIRT_PATCHED_GEANT4`, the configure check, the tag scheme, per-build libraries);
  - Stock-build limits (with numbers and evidence paths);
  - Switching back to the patched build (link to the unchanged recipe);
  - New ledger fields;
  - the updated test count and time.

  Replace the claim that the patched build is required.
- [ ] **Step 3:** Correct the UHDR claim in `docs/dose-rate/mesoscopic-feasibility.md:25`. UHDR registers `G4ChemReboundTransportation` (`EmDNAChemistry.cc:352`), so it uses the kernel fold. It also shares the contact gap; this is an upstream-report candidate, not fixed here.
- [ ] **Step 4:** `re-enabling-patch.md`, a checklist:
  1. apply the three patches per their READMEs;
  2. build and install to a separate prefix;
  3. configure with `-DPTIRT_PATCHED_GEANT4=ON -DGeant4_DIR=<prefix>/lib/cmake/Geant4`;
  4. confirm the configure check passes;
  5. recapture libraries (patched tag);
  6. run ctest and check `loadedGeant4Modules` points at the prefix;
  7. rerun `RunBuildComparison.ps1`.

  `upstream-notes.md`: the stock fold defect (with values), the missing trials for fresh sources, distance-zero partner suppression and the stale `fSampledMinTimeStep` quirk, each with its file:line. Mark it "prepared, not submitted".
- [ ] **Step 5:** Run `git diff --stat main -- patches/` (expect empty), and `node docs/explorer/build.js` only if repo-root `src/`/`header/` changed (they should not). Commit with `docs(ptirt): ADR 0010, stock-build README and checklist`.

---

## Final verification (before claiming done)

- `build-stock`: configure, build and the full ctest PASS. `loadedGeant4Modules` lies only under `${G4_ROOT}/bin`.
- `build-patched`: full ctest PASS, or **NOT RUN** with the reason.
- The ledger shows `buildMode`, `newOldContacts`, `injected.coincidentMolecules` and `boxFoldGuard`.
- The comparison, overlap and separated results are in `.scratch/…` and summarised in ADR 0010 with their limits.
- `patches/geant4/11.4.3/*` is unchanged.

## Deviations from the spec to flag at approval

1. **Same physics seeds, per-build libraries.** The spec asks for "stock vs patched on the same libraries". The plan uses the same physics seeds, with each build capturing its own library: stock after the startup contacts, patched at chemistry start. Reason: a post-startup packet in the patched build would get a second trial on every new-new pair through `IsSourceContact`. Each build therefore refuses the other's library. Physical identity is proven by equal `chemistryStartDigest` values.
2. **Gate G2.** Products of one reaction are placed at one point (`G4DNAMakeReaction`). If the spike finds coincident molecules after the startup step, the strict capture guard would refuse ordinary packets. The plan stops at G2 rather than choosing a fix.
