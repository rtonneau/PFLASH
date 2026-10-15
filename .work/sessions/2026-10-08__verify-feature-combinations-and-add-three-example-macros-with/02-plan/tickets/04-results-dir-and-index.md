# Ticket 04: results-dir-and-index

**Model:** sonnet
**Model (Jev):** sonnet-5.5 (confidence 0.43)
**Effort:** high

**Acceptance Criteria:**
- [ ] When neither `--dir` nor `/run/outputDir` is given, output goes to `<exeDir>/results`; both still override, and the tests that rely on `OutputDir` pass.
- [ ] After each dump, `<outdir>/Manifest.json` lists every dump so far (folder or prefix, events, seed, beam) and is rewritten each time; per-dump manifests are unchanged.
- [ ] The index builder is pure logic on `DataNode`/`JsonWriter` with a unit test; the index also lists the exit-time `EndOfRun_` flush.
- [ ] A failed open of the index is a `JustWarning`, like the per-dump manifest.

**Files to Touch:**
- `src/core/OutputDir.cc`
- `sim.cc`
- `src/scoring/ResultsIndex.cc`
- `header/scoring/ResultsIndex.hh`
- `src/scoring/RunManifest.cc`
- `src/scoring/RunAccumulatorMessenger.cc`
- `test/ResultsIndexTest.cc`
- `CMakeLists.txt`

**Verification Step:**

Run:
```bash
cd build-ninja && ctest --output-on-failure; cd ../build && ./sim beam.in --threads 2 && ls results && head -40 results/Manifest.json
```

Expected:
ctest all passed; `results/` holds the data files or sub-folders plus an index `Manifest.json`.

**Notes:**

Keep `OutputDir` pure (the exe directory is passed in from `sim.cc`). Collision: with `/run/dumpDataAndReset` (no subdir) the per-dump manifest is also `results/Manifest.json`; decide in the code, document it, and make the index key the dump entries clearly (e.g. a top-level `dumps` array) so the two cannot be mistaken. A flat dump overwriting the index is not acceptable.
