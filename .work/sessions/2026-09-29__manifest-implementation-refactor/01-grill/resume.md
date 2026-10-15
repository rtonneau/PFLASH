# Session: Manifest implementation refactor

**Date:** 2026-09-29T13:28:00.241Z
**Status:** Grill phase complete

## Problem Statement

`ManifestWriter::Write(std::ostream&, const ManifestData::Manifest&)` hard-codes every manifest key (18 top-level keys plus the per-run object). Each entry is spelled out in four places: the `ManifestData::Manifest` struct, the collector `RunManifest::Write`, the writer, and the tests. Adding or removing an entry means editing all of them. The goal: entries are registered as data (ordered maps/vectors), and the serialiser never changes when an entry is added or removed.

## Context & Constraints

- Current files: `header/scoring/ManifestData.hh` (`Beam`, `RunRecord`, `Scavenger`, `Manifest`), `src/scoring/ManifestWriter.cc` (+ `.hh`), `src/scoring/RunManifest.cc` (Geant4-facing collector), `test/ManifestWriterTest.cc`, ADR `docs/adr/0005-manifest-per-dump.md`.
- `ManifestData::RunRecord` / `Beam` are also used by the run pipeline: `PrimaryGeneratorAction` (samples the beam), `Run` (`SetBeamIfUnset`/`GetBeam`, carried through `Run::Merge`), `RunAction::EndOfRunAction` (builds a RunRecord), `RunAccumulator` (`AddRunRecord`/`GetRunRecords`), `test/RunAccumulatorTest.cc`.
- Portable-class convention: new logic units are standard-library only and copy-paste portable (include prefix adjusted on copy).
- ADR 0005: changing the meaning of a field bumps `schemaVersion`; this refactor changes no meaning.
- Unit tests are plain `assert` + CTest, built and run from `build-ninja/` (Debug).

## Success Metrics

- Adding or removing a manifest entry means editing one `Add(...)` line in `RunManifest.cc` and nothing in the serialiser.
- `Manifest.json` has the same keys, key order and values as today (numbers at 12 significant digits, NaN/inf as `null`, beam fields `null` when a run has no beam). Only the whitespace layout changes; `schemaVersion` stays 1.
- All ctest targets pass in `build-ninja/`; `sim beam.in` (10 keV e-, `/run/beamOn 2`) writes a valid `Manifest.json`.

## Architecture & Approach

1. **`DataNode.{hh,cc}`** (`scoring/`, portable, standard library only): a format-neutral ordered tree with null, bool, integer, double, string, array and object. No JSON in its name or API. Objects keep insertion order; `Add` on an existing key replaces the value in place, keeping its position.
2. **`JsonWriter.{hh,cc}`** (`scoring/`, portable): generic recursive `Write(std::ostream&, const DataNode&)` and `EscapeJson`. Numbers use 12 significant digits, NaN/inf become `null`, integers stay integers. Layout: an object/array whose children are all scalars prints on one line; otherwise it prints across lines with a 2-space indent. It is the only backend: no abstract writer interface or format switch until a second format exists (a later `YamlWriter` would be one new file reading the same tree). `ManifestWriter.{hh,cc}` is deleted.
3. **`RunManifest.cc`** is the only file that lists manifest keys:
   - `Write(prefix, subdir, files)`: sequential top-level `Add` calls in the current key order (schemaVersion, timestamp, geant4Version, macro, chemistry, scavengers, pH, chemistryEndTime_ns, runMode, threads, outputDirAsConfigured, outputDirAbsolute, prefix, subdir, totalEvents, totalEnergyDeposit_eV, files, runs). Scavengers are built inline. `totalEvents` and `totalEnergyDeposit_eV` come from `RunAccumulator` totals; `runs` comes from the stored run nodes. Then `JsonWriter::Write` into `OutputDir::Resolve("Manifest.json")` (a failed open stays a `JustWarning`).
   - new `RecordRun(const Run&)`: builds one run node (`run`, `events`, `particle`, `beamEnergy_keV`, `position_um`, `direction` or `null`s when the run has no beam, `energyDeposit_eV`, `seed`) and stores it through `RunAccumulator`. `RunAction::EndOfRunAction` calls it in place of the 8 `RunRecord` lines.
4. **`RunAccumulator`**: stores `std::vector<DataNode>` run entries in place of `RunRecord`, and gains a total event count fed through `Accumulate(...)`. `ClearAccumulated` resets both.
5. **`Beam`** moves into `header/actions/Run.hh`. `ManifestData.hh` and `RunRecord` are deleted.
6. **Tests**: new `test/JsonWriterTest.cc` (escaping, NaN → `null`, empty containers, the inline/expanded layout rule, duplicate-key replacement, integer vs double formatting). `test/RunAccumulatorTest.cc` is updated for run nodes and the event total. `test/ManifestWriterTest.cc` is removed.
7. **Docs**: amend ADR 0005's writer paragraph (the collector builds a format-neutral ordered tree; `JsonWriter` serialises it generically). Update the `CLAUDE.md` Key Files entries (`ManifestWriter`, `RunAccumulator`, `RunManifest`) and the `sim-output` skill. `CONTEXT.md` is unchanged (the **Manifest** term is not affected).

## Assumptions & Trade-offs

- A generic tree was chosen over a typed struct plus field table (3 edit sites per entry) and over a provider registry spread across subsystems (key order and entry list scattered).
- Per-run entries are built in `RunManifest::RecordRun`, not inline in `RunAction`, so that all manifest key names and units live in one file and `RunAction` doesn't define the output format.
- The null-beam branch and the key order now live in Geant4-facing code and lose their unit tests; the whole-manifest key-order test is dropped, since the order is simply the order of the `Add` calls. Accepted: both are trivial and visible in source.
- Totals come from typed accumulators in `RunAccumulator`, not from reading values back out of the run nodes.
- Duplicate keys replace in place rather than assert (asserts are compiled out in the RelWithDebInfo `sim` build).
- Whitespace layout changes (each run object becomes multi-line). This is not a schema change.

## Open Questions

- None blocking. The name `DataNode` was proposed and not objected to.

## Notes

- `RunAccumulator` stays kernel-free pure logic (it stores the portable `DataNode`).
- Memory note: `G4UnitDefinition::GetValueOf()` must not be called from Debug test binaries; the new tests don't need units.

## Token Usage

- **Input:** 48
- **Output:** 16794
- **Cache read:** 1528422
- **Cache creation:** 33643
- **Total:** 1578907
