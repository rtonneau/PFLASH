# Ticket 04: results-dir-and-index

**Status:** ✅ Done

## Local Test Result

`ctest --test-dir build-ninja`: 16/16 passed (re-run in this session), including ResultsIndexTest and the extended OutputDirTest. Subagent runs: shortened beam.in (10 keV, 2 events, 1 ms end time, --threads 2, no --dir) put output in build/results with an index Manifest.json listing the EndOfRun_ dump; a macro with two `/run/dumpDataAndResetToDir` (sub_01, sub_02) plus the EndOfRun_ flush gave an index listing all three, each subfolder with its own full Manifest.json; a flat dump plus ToDir gave a root Manifest.json that is both the flat manifest and an index.

## Review Notes

All four criteria hold. The verification command's full beam.in (100 keV, 1 s end time) was killed after ~25 min, so it was verified only in shortened form. Collision rule: a flat dump with empty prefix writes its manifest to the index path, so that file is the dump manifest plus a top-level `dumps` array (no `kind` key); this is the one exception to "per-dump manifests unchanged", documented in ResultsIndex.hh, RunManifest.hh and an ADR 0005 addendum. The default dir is applied lazily under a mutex (first Resolve can come from a worker); a later /run/outputDir naming a different directory after use is refused. CLAUDE.md output paragraph and the sim-output skill were updated; the "sim.cc prepends macro/" sentence in CLAUDE.md is still stale and is fixed in ticket 05.

## Blockers / Challenges

None beyond the slow full beam.in.

## Commits

- 0bb1bf3 feat(scoring): default results dir next to the exe and a results index (ticket 04)

## Time Spent

22m (ticket-start.js to ticket-complete.js)
