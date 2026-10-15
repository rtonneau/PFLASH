# Run Manifest (one Manifest.json per Dump) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans. Steps use checkbox (`- [ ]`) syntax.

**Goal:** Every dump (`/run/dumpDataAndReset`, `/run/dumpDataAndResetToDir`, exit-time `EndOfRun_` flush) writes one `Manifest.json` beside its data; `EnergyDeposit.Txt` is removed.

**Architecture:** A portable, stream-only `ManifestWriter` serialises a plain-data `ManifestData::Manifest`. The per-run beam is sampled from the real gun on a worker (first event), carried on `Run`, merged in `Run::Merge`, recorded per run in `RunAccumulator`. A thin Geant4-facing `RunManifest` collector fills the struct at dump time and writes the file from `RunAccumulatorMessenger::WriteAllAndReset`.

**Tech Stack:** C++20, Geant4 11.4.1, CMake/Ninja, plain `assert` + CTest.

**Spec:** `.work/sessions/2026-09-29__run-manifest-file/01-grill/resume.md`; ADR `docs/adr/0005-manifest-per-dump.md`; glossary terms **Dump**/**Manifest** in `CONTEXT.md`.

## Global Constraints

- New logic classes are copy-paste portable: `ManifestData.hh`/`ManifestWriter.*` include only the standard library (no `globals.hh`, no project headers). Includes rooted at `header/` (`#include "scoring/ManifestWriter.hh"`).
- Style: PascalCase files/classes, camelCase variables, match surrounding file (Geant4 types in Geant4-facing files, 4-space indent in `RunAccumulatorMessenger.cc`/`Run.cc`, 2-space in `RunAccumulator`/`OutputDir`).
- Unit tests build and run from `build-ninja/` (Debug), see `.claude/geant4-instructions.md` §5. Smoke test is run from `build/` (RelWithDebInfo): 10 keV e-, `/run/beamOn 2`, macro written into `<runBuildDir>/macro` (untracked), ending with `/run/dumpDataAndReset`.
- Use `DnaLogger` for diagnostics, except the existing plain `G4cout` success-marker line in `WriteAllAndReset` (leave it).
- Manifest write failure is a `JustWarning` `G4Exception`, never fatal. Manifest is excluded from its own `files` list; MT per-event `output_event_*` files are not listed.
- Old `.claude/plans/*` keep their historical `EnergyDeposit.Txt` mentions.
- No git commit hash and no app-version field (project has no version; resume open question resolved: omit).

## File Structure

| File | Action | Responsibility |
|---|---|---|
| `header/scoring/ManifestData.hh` | Create | Header-only plain structs: `Beam`, `RunRecord`, `Scavenger`, `Manifest` |
| `header/scoring/ManifestWriter.hh`, `src/scoring/ManifestWriter.cc` | Create | `Write(std::ostream&, const Manifest&)`, `EscapeJson`; derives totals from `runs` |
| `test/ManifestWriterTest.cc` | Create | Exact-output and escaping tests |
| `header/scoring/RunAccumulator.hh`, `src/scoring/RunAccumulator.cc` | Modify | `AddRunRecord`, `GetRunRecords`; cleared by `ClearAccumulated` |
| `header/actions/Run.hh`, `src/actions/Run.cc` | Modify | `Beam`, seed on `Run`; merge in `Merge` |
| `src/actions/PrimaryGeneratorAction.cc` | Modify | Snapshot gun into current `Run` on first event |
| `src/actions/RunAction.cc` | Modify | Build `RunRecord` in master `EndOfRunAction` |
| `header/core/OutputDir.hh`, `src/core/OutputDir.cc` | Modify | `GetDirectory()` |
| `header/scoring/RunManifest.hh`, `src/scoring/RunManifest.cc` | Create | `SetMacroName`, `Write(prefix, subdir, files)` collector |
| `src/scoring/RunAccumulatorMessenger.cc` | Modify | Track files written, call `RunManifest::Write`, drop `EnergyDeposit.Txt` |
| `sim.cc` | Modify | `RunManifest::SetMacroName(macroFile)` |
| `CMakeLists.txt` | Modify | `ManifestWriterTest`; add `RunAccumulatorTest` sources if needed |
| `.claude/.claude-project.json`, `.claude/skills/sim-output/SKILL.md`, `CLAUDE.md` | Modify | Replace `EnergyDeposit.Txt` with `Manifest.json` |

## Task 1: ManifestData + ManifestWriter (portable, tested)

**Files:** Create `header/scoring/ManifestData.hh`, `header/scoring/ManifestWriter.hh`, `src/scoring/ManifestWriter.cc`, `test/ManifestWriterTest.cc`; modify `CMakeLists.txt`; commit the already-written `CONTEXT.md` + `docs/adr/0005-manifest-per-dump.md`.

**Interfaces — Produces:**
```cpp
// header/scoring/ManifestData.hh  (std only)
namespace ManifestData {
struct Beam { std::string particle; double energy_keV = 0.; double position_um[3] = {0., 0., 0.}; double direction[3] = {0., 0., 1.}; };
struct RunRecord { int runId = 0; long events = 0; bool hasBeam = false; Beam beam; long seed = 0; double energyDeposit_eV = 0.; };
struct Scavenger { std::string species; double molarity_M = 0.; };
struct Manifest {
  int schemaVersion = 1;
  std::string timestamp, geant4Version, macro, chemistry;
  std::vector<Scavenger> scavengers;
  double pH = 7.; double chemistryEndTime_ns = 0.;
  std::string runMode; int threads = 1;
  std::string outputDirAsConfigured, outputDirAbsolute, prefix, subdir;
  std::vector<std::string> files;
  std::vector<RunRecord> runs;
};
}
// header/scoring/ManifestWriter.hh
namespace ManifestWriter {
  std::string EscapeJson(const std::string& text);            // ", \, \n, \r, \t, other <0x20 as \u00XX
  void Write(std::ostream& out, const ManifestData::Manifest& manifest);
}
```
`Write` emits 2-space-indented JSON in this key order: `schemaVersion, timestamp, geant4Version, macro, chemistry, scavengers, pH, chemistryEndTime_ns, runMode, threads, outputDirAsConfigured, outputDirAbsolute, prefix, subdir, totalEvents, totalEnergyDeposit_eV, files, runs`. `totalEvents` and `totalEnergyDeposit_eV` are summed from `runs` (never stored). Doubles print with `std::setprecision(12)` general format; non-finite prints `null`. A run with `hasBeam == false` prints `null` for `particle`, `beamEnergy_keV`, `position_um`, `direction`. Empty arrays print `[]`. Each `runs`/`scavengers` element is one line: `{"run": 0, "events": 2, "particle": "e-", "beamEnergy_keV": 10, "position_um": [0, 0, 0], "direction": [0, 0, 1], "energyDeposit_eV": 123.5, "seed": 12345}`; scavenger: `{"species": "O2", "molarity_M": 0.000273}`.

- [ ] **Step 1: Write the failing test** `test/ManifestWriterTest.cc` (plain assert, same style as `test/PhysicsInteractionCounterTest.cc`):
```cpp
#include "scoring/ManifestWriter.hh"
#include <cassert>
#include <iostream>
#include <sstream>

static std::string Render(const ManifestData::Manifest& m) { std::ostringstream s; ManifestWriter::Write(s, m); return s.str(); }

static void TestEscapeJson()
{
  assert(ManifestWriter::EscapeJson("a\"b\\c\nd\te") == "a\\\"b\\\\c\\nd\\te");
  assert(ManifestWriter::EscapeJson(std::string(1, '\x01')) == "\\u0001");
}

static void TestTotalsAreSummedFromRuns()
{
  ManifestData::Manifest m;
  ManifestData::RunRecord a; a.runId = 0; a.events = 2; a.energyDeposit_eV = 100.;
  ManifestData::RunRecord b; b.runId = 1; b.events = 3; b.energyDeposit_eV = 50.5;
  m.runs = {a, b};
  const std::string out = Render(m);
  assert(out.find("\"totalEvents\": 5,") != std::string::npos);
  assert(out.find("\"totalEnergyDeposit_eV\": 150.5,") != std::string::npos);
}

static void TestRunLineWithBeam()
{
  ManifestData::Manifest m;
  ManifestData::RunRecord r; r.runId = 0; r.events = 2; r.hasBeam = true; r.seed = 12345; r.energyDeposit_eV = 123.5;
  r.beam.particle = "e-"; r.beam.energy_keV = 10.;
  m.runs = {r};
  assert(Render(m).find("{\"run\": 0, \"events\": 2, \"particle\": \"e-\", \"beamEnergy_keV\": 10, "
                        "\"position_um\": [0, 0, 0], \"direction\": [0, 0, 1], "
                        "\"energyDeposit_eV\": 123.5, \"seed\": 12345}") != std::string::npos);
}

static void TestRunWithoutBeamPrintsNull()
{
  ManifestData::Manifest m;
  ManifestData::RunRecord r; r.runId = 4; r.events = 1;
  m.runs = {r};
  assert(Render(m).find("\"particle\": null, \"beamEnergy_keV\": null, \"position_um\": null, \"direction\": null") != std::string::npos);
}

static void TestEmptyArraysAndEscapedStrings()
{
  ManifestData::Manifest m;
  m.macro = "C:\\macro\\a\"b.in";
  const std::string out = Render(m);
  assert(out.find("\"runs\": []") != std::string::npos);
  assert(out.find("\"files\": []") != std::string::npos);
  assert(out.find("\"scavengers\": []") != std::string::npos);
  assert(out.find("\"macro\": \"C:\\\\macro\\\\a\\\"b.in\"") != std::string::npos);
  assert(out.find("\"schemaVersion\": 1,") != std::string::npos);
}

int main()
{
  TestEscapeJson(); TestTotalsAreSummedFromRuns(); TestRunLineWithBeam();
  TestRunWithoutBeamPrintsNull(); TestEmptyArraysAndEscapedStrings();
  std::cout << "ManifestWriterTest: all tests passed" << std::endl;
  return 0;
}
```
- [ ] **Step 2:** In `CMakeLists.txt`, after the `PhysicsInteractionCounterTest` block:
```cmake
# Standalone unit test for ManifestWriter (portable JSON serialiser, std only).
add_executable(ManifestWriterTest test/ManifestWriterTest.cc src/scoring/ManifestWriter.cc)
target_include_directories(ManifestWriterTest PRIVATE ${project_include_dirs})
add_test(NAME ManifestWriterTest COMMAND ManifestWriterTest)
```
Build from `build-ninja/` per `.claude/geant4-instructions.md`. Expected: FAIL to compile/link (`ManifestWriter.cc` missing).
- [ ] **Step 3:** Create `ManifestData.hh` (above), `ManifestWriter.hh`, and `ManifestWriter.cc` implementing `EscapeJson` and `Write` per the format above (helpers in an anonymous namespace: `Number(double)`, `Vec3(const double*)`, `Str(const std::string&)`).
- [ ] **Step 4:** Build and run `ctest --test-dir build-ninja --output-on-failure -R ManifestWriterTest`. Expected: `100% tests passed`.
- [ ] **Step 5:** `git add header/scoring/ManifestData.hh header/scoring/ManifestWriter.hh src/scoring/ManifestWriter.cc test/ManifestWriterTest.cc CMakeLists.txt CONTEXT.md docs/adr/0005-manifest-per-dump.md` then commit `feat: add portable ManifestWriter and manifest ADR/glossary`.

## Task 2: RunAccumulator keeps per-run records

**Files:** Modify `header/scoring/RunAccumulator.hh`, `src/scoring/RunAccumulator.cc`, `test/RunAccumulatorTest.cc`.

**Interfaces — Consumes:** `ManifestData::RunRecord` (Task 1). **Produces:**
```cpp
void AddRunRecord(const ManifestData::RunRecord& record);            // appends; does not set pending flag
const std::vector<ManifestData::RunRecord>& GetRunRecords();
// ClearAccumulated() also empties the record list.
```
- [ ] **Step 1:** Add to `test/RunAccumulatorTest.cc` (register in `main`):
```cpp
static void TestRunRecordsAccumulateAndClear()
{
  RunAccumulator::ClearAccumulated();
  ManifestData::RunRecord r; r.runId = 3; r.events = 2;
  RunAccumulator::AddRunRecord(r);
  r.runId = 4;
  RunAccumulator::AddRunRecord(r);
  assert(RunAccumulator::GetRunRecords().size() == 2);
  assert(RunAccumulator::GetRunRecords()[1].runId == 4);
  RunAccumulator::ClearAccumulated();
  assert(RunAccumulator::GetRunRecords().empty());
}
```
- [ ] **Step 2:** Build `RunAccumulatorTest` (Debug), expected FAIL (no `AddRunRecord`).
- [ ] **Step 3:** Implement with a `std::vector<ManifestData::RunRecord> gRunRecords` in the anonymous namespace, `#include "scoring/ManifestData.hh"` and `<vector>` in the header; clear it in `ClearAccumulated`.
- [ ] **Step 4:** `ctest --test-dir build-ninja --output-on-failure` all Passed.
- [ ] **Step 5:** Commit `feat: RunAccumulator stores per-run records`.

## Task 3: Capture beam + seed per run and record it

**Files:** Modify `header/actions/Run.hh`, `src/actions/Run.cc`, `src/actions/PrimaryGeneratorAction.cc`, `src/actions/RunAction.cc`. Verified by smoke run (Geant4-object code, no unit test, per project convention).

**Interfaces — Consumes:** `ManifestData::Beam`, `RunAccumulator::AddRunRecord`. **Produces:** `Run::SetBeamIfUnset(const ManifestData::Beam&)`, `Run::HasBeam()`, `Run::GetBeam()`, `Run::GetSeed()`.

- [ ] **Step 1:** `Run.hh`: `#include "scoring/ManifestData.hh"`; add public
```cpp
    void SetBeamIfUnset(const ManifestData::Beam &beam) { if (!fHasBeam) { fBeam = beam; fHasBeam = true; } }
    G4bool HasBeam() const { return fHasBeam; }
    const ManifestData::Beam &GetBeam() const { return fBeam; }
    long GetSeed() const { return fSeed; }
```
and private `G4bool fHasBeam = false; ManifestData::Beam fBeam; long fSeed = 0;`.
- [ ] **Step 2:** `Run.cc`: include `"Randomize.hh"`; end of `Run::Run()` body: `fSeed = G4Random::getTheEngine()->getSeed();`. In `Run::Merge`, after `fSumEne += ...`: `if (localRun->fHasBeam) SetBeamIfUnset(localRun->fBeam);`.
- [ ] **Step 3:** `PrimaryGeneratorAction.cc`: include `"actions/Run.hh"`, `"G4RunManager.hh"`, `"G4ThreeVector.hh"`; after `GeneratePrimaryVertex`:
```cpp
  auto *run = dynamic_cast<Run *>(G4RunManager::GetRunManager()->GetNonConstCurrentRun());
  if (run != nullptr && !run->HasBeam())
  {
    ManifestData::Beam beam;
    beam.particle = fpParticleGun->GetParticleDefinition()->GetParticleName();
    beam.energy_keV = fpParticleGun->GetParticleEnergy() / keV;
    const G4ThreeVector pos = fpParticleGun->GetParticlePosition();
    const G4ThreeVector dir = fpParticleGun->GetParticleMomentumDirection();
    beam.position_um[0] = pos.x() / micrometer; beam.position_um[1] = pos.y() / micrometer; beam.position_um[2] = pos.z() / micrometer;
    beam.direction[0] = dir.x(); beam.direction[1] = dir.y(); beam.direction[2] = dir.z();
    run->SetBeamIfUnset(beam);
  }
```
- [ ] **Step 4:** `RunAction.cc` master `EndOfRunAction`, right after `RunAccumulator::Accumulate(...)`:
```cpp
        ManifestData::RunRecord record;
        record.runId = run->GetRunID();
        record.events = nofEvents;
        record.hasBeam = masterRun->HasBeam();
        record.beam = masterRun->GetBeam();
        record.seed = masterRun->GetSeed();
        record.energyDeposit_eV = masterRun->GetSumDose() / eV;
        RunAccumulator::AddRunRecord(record);
```
add `#include "G4SystemOfUnits.hh"`.
- [ ] **Step 5:** Build `build/` (RelWithDebInfo), no macro run yet. Expected: compiles. Commit `feat: sample beam and seed per run into RunAccumulator`.

**Notes:** `G4Random::getTheEngine()->getSeed()` returns the CLHEP `theSeed`. Task 4's smoke includes a `/random/setSeeds 111 222` case; if the printed seed does not change, fall back to `G4Random::getTheSeeds()[0]` and say so in the commit message.

## Task 4: RunManifest collector + dump integration, remove EnergyDeposit.Txt

**Files:** Modify `header/core/OutputDir.hh`, `src/core/OutputDir.cc`, `test/OutputDirTest.cc`, `sim.cc`, `src/scoring/RunAccumulatorMessenger.cc`; create `header/scoring/RunManifest.hh`, `src/scoring/RunManifest.cc`. (`file(GLOB_RECURSE)` picks up new sources.)

**Interfaces — Produces:** `G4String OutputDir::GetDirectory()` (configured dir as given, empty if none); `void RunManifest::SetMacroName(const G4String&)`; `void RunManifest::Write(const G4String& prefix, const G4String& subdir, const std::vector<std::string>& files)`.

- [ ] **Step 1:** `OutputDirTest.cc`: after `Configure(<tmp dir>)` assert `OutputDir::GetDirectory()` equals that string; with `Configure("")` assert it is `""`. Build `OutputDirTest`, expect FAIL.
- [ ] **Step 2:** Implement `GetDirectory()` (return the existing file-scope directory string). Debug tests pass.
- [ ] **Step 3:** `RunManifest.cc` `Write`: build `ManifestData::Manifest`: `timestamp` ISO-8601 local (`std::put_time`, `"%Y-%m-%dT%H:%M:%S"`); `geant4Version` = `G4VERSION_TAG` with the leading `"$Name: "` and trailing `" $"` stripped (`#include "G4Version.hh"`); `macro` from `SetMacroName`; `chemistry` = `ChemistryRegistry::Selected()->name` (empty if null); `runMode`/`threads`: `dynamic_cast<G4MTRunManager*>(G4RunManager::GetRunManager())` → `"MT"` and `GetNumberOfThreads()`, else `"Serial"` and 1; `chemistryEndTime_ns = G4Scheduler::Instance()->GetEndTime() / ns`; `pH`/`scavengers` from `dynamic_cast<DnaChemistryWorld*>(static_cast<const DetectorConstruction*>(G4RunManager::GetRunManager()->GetUserDetectorConstruction())->GetChemistryWorld())` via `GetpH()` and `GetScavengers()` (skip when null); `outputDirAsConfigured = OutputDir::GetDirectory()`; `outputDirAbsolute = std::filesystem::absolute(dir.empty() ? "." : dir).lexically_normal().string()`; `prefix`, `subdir`, `files` as passed; `runs = RunAccumulator::GetRunRecords()`. Then `std::ofstream out(OutputDir::Resolve("Manifest.json"))`; if `!out` → `G4Exception("RunManifest::Write", "ManifestWriteFailed", JustWarning, ...)`; else `ManifestWriter::Write(out, m)`.
- [ ] **Step 4:** `RunAccumulatorMessenger.cc` `WriteAllAndReset`: add `std::vector<std::string> files;` and after each write `files.push_back(prefix + "<name>")` — `Species.Txt`, `Species_nt_species.csv`, `Species_nt_species_all.csv` (only inside the `scorer != nullptr` branch), `Reactions.Txt`, `Reactions_nt_reactions.csv`, `ReactionsMetadata.csv`, `PhysicsInteractions.Txt`, `PhysicsInteractions.csv`. Delete the whole `// Energy` block (`EnergyDeposit.Txt`). Just before `RunAccumulator::ClearAccumulated();` add `RunManifest::Write(prefix, subdir, files);`. Update the guidance string at the top of the file (`EnergyDeposit` → `Manifest`). `RunAccumulator::GetAccumulatedEnergy` stays (unit-tested; totals now come from run records).
- [ ] **Step 5:** `sim.cc`: `#include "scoring/RunManifest.hh"`; in the batch branch after computing `macroFile`: `RunManifest::SetMacroName(macroFile);`.
- [ ] **Step 6 (smoke, `build/`):** scratch macro in `<runBuildDir>/macro/manifest_smoke.in` (untracked): `/run/initialize`, `/gun/particle e-`, `/gun/energy 10 keV`, `/run/beamOn 2`, `/gun/energy 25 keV`, `/random/setSeeds 111 222`, `/run/beamOn 1`, `/run/dumpDataAndReset`, `/run/beamOn 1`, `/run/dumpDataAndResetToDir second`. Run `./sim manifest_smoke.in --dir smoke/m1` (Serial), then once with `--threads 2`. Expected in `smoke/m1/Manifest.json`: two `runs` (10 keV/2 events; 25 keV/1 event with different `seed`) and `"totalEvents": 3`; `smoke/m1/second/Manifest.json` has one run; no `EnergyDeposit.Txt`; `files` lists exist on disk; MT output has the same beam fields. `successMarkers` from `.claude/.claude-project.json` appear.
- [ ] **Step 7:** Debug `ctest` all Passed. Commit `feat: write Manifest.json on every dump, remove EnergyDeposit.Txt`.

## Task 5: Docs and project metadata

**Files:** Modify `.claude/.claude-project.json` (replace `"EnergyDeposit.Txt"` in `run.outputs` with `"Manifest.json"`), `.claude/skills/sim-output/SKILL.md` (replace the `EnergyDeposit.Txt` bullet with a `Manifest.json` bullet: per-dump JSON, `runs[]`, fields, file list, `schemaVersion`), `CLAUDE.md` (Key Files: add `ManifestWriter`, `RunManifest`; note the `RunAccumulator` per-run records; Macro/Output paragraph mentions `Manifest.json`).

- [ ] **Step 1:** Apply the three edits.
- [ ] **Step 2:** `grep -rn EnergyDeposit --include=* . --exclude-dir=build --exclude-dir=build-ninja --exclude-dir=.work --exclude-dir=.scratch --exclude-dir=plans` returns only `RunAccumulator`'s `GetAccumulatedEnergy` and `ScoreSpecies.cc:68` lines (no file mentions of `EnergyDeposit.Txt`).
- [ ] **Step 3:** Commit `docs: document Manifest.json, drop EnergyDeposit.Txt references`.

## Self-Review

- Spec coverage: per-dump manifest (T4), JSON + units + schemaVersion (T1), all fields (T1/T4: git commit and app version deliberately omitted), beam capture via worker gun/`Run::Merge` (T3), per-run seed (T3), portable writer + thin collector (T1/T4), warn-on-failure (T4), `EnergyDeposit.Txt` removal and doc updates (T4/T5), CONTEXT/ADR (T1). Files list relative, manifest excluded (T4).
- Deviation from resume: beam is sampled on the first `GeneratePrimaries` of the run on any worker (not "thread 0 at run start") so that a worker that gets zero events cannot leave the run beamless.
- Types: `Beam`, `RunRecord`, `Manifest`, `Scavenger` names identical across tasks; `energyDeposit_eV` computed as `GetSumDose() / eV`.

## Verification (end to end)

`ctest --test-dir build-ninja --output-on-failure` (all Passed incl. `ManifestWriterTest`, `RunAccumulatorTest`, `OutputDirTest`), then the Task 4 smoke run, Serial and `--threads 2`.
