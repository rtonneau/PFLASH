# Ticket 05: example-macros-and-docs

**Status:** ✅ Done

## Local Test Result

Subagent ran example_sbs.in, example_irt.in and example_meso.in from build/ (--dir into the scratch dir): all exit normally, no fatal; each output has an index Manifest.json with sub_01/ and sub_02/. Manifest checks: sbs -> timeStepModel SBS, mesoEnabled false, end time 1000 ns; irt -> IRT_syn, false, 1000 ns; meso -> IRT_syn+mesoscopic, true, 1e6 ns, SpeciesMeso.* written. Seeds differ across sub-runs (12345, 23456), so `/random/setSeeds` between beamOn works.

## Review Notes

Read the SBS macro in full, diffed it against the meso macro, and read ADR 0008: content matches the grill decisions (O2 21 % with a comment-out line for pure water, two sub-runs of 2 events, dumps to sub_01/sub_02, meso example smoke-sized with a 1 ms end time). CLAUDE.md, README.md and the sim-output skill no longer say SBS is unsupported. CONTEXT.md unchanged. The meso run prints only the known benign WrongResolution warnings.

## Blockers / Challenges

None.

## Commits

- f4003ef docs: example macros per chemistry mode, ADR 0008, mode and lookup docs (ticket 05)

## Time Spent

6m (ticket-start.js to ticket-complete.js)
