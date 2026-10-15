# Ticket 05: recipe-export-data-as-hdf5

**Status:** ✅ Done

## Local Test Result

`grep -q "^## Serial vs MT" docs/recipes/export-data-as-hdf5.md && grep -q "mutex" docs/recipes/export-data-as-hdf5.md && grep -q "MesoSpatialFile" docs/recipes/export-data-as-hdf5.md && echo OK` → `OK` (re-ran myself).

## Review Notes

Subagent (sonnet) wrote `docs/recipes/export-data-as-hdf5.md`, grounded in a full read of `MesoSpatialFile.cc/.hh` and the call sites in `TimeStepAction.cc:634-653` and `RunAccumulatorMessenger.cc:243-259`. Documents the generic stage/move/safety-net pattern, cites the exact layout convention with a link to `docs/output/SpeciesMesoSpatial-h5.md` instead of repeating it, and the Serial vs MT section correctly explains why this recipe diverges from the `Run::Merge` pattern (one shared file handle has no per-worker-then-merge split; a process-wide `std::mutex` serializes appends instead) with an explicit contrast to tickets 2-4/10. No changes needed.

## Blockers / Challenges

None.

## Commits

- 32e7555 docs: export-data-as-hdf5 recipe (ticket 05)

## Time Spent

2m (ticket-start.js to ticket-complete.js)
