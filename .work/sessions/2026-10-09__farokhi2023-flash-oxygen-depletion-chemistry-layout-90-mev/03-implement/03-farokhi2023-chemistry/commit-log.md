# Ticket 03: farokhi2023-chemistry

**Status:** ✅ Done

## Local Test Result

Subagent ran the Verification Step (build, then `./sim.exe scratch_farokhi_smoke.in --dir .../ticket03-smoke`): exit 0, no `EEEE`/`FatalException`, log's `/chem/list` output includes `Farokhi2023`, `Manifest.json` records `"chemistry": "Farokhi2023"`, `Species.Txt` non-empty with zero O2/O2m/HO2/O3m yield at the anoxic smoke condition.

This session re-ran `cmake --build build --config RelWithDebInfo --target sim` (via vcvars64): `ninja: no work to do`, already current. Spot-checked `build/ticket03-smoke.log` directly: `Available chemistries: PureWater (default) BoscoloChem Tonneau2025 Farokhi2023`. Spot-checked `Manifest.json`: `"chemistry": "Farokhi2023"`, `"chemistryModel": "IRT_syn+mesoscopic"`.

## Review Notes

Read `Farokhi2023Reactions.hh`/`.cc` in full against the Acceptance Criteria's explicit rate/product/type list: all 9 base reactions present with the exact rates and types specified (type_1/type_3 → `add()`, type_2/type_4 → `partial()`), no reaction touching O2/Om/O3m/HO2/HO2m/O2m in the main table. `BuildFarokhiBulkReactions()` returns exactly the 3 specified `BulkReactionEntry` values with matching rates/products/unit convention. Structure matches `BoscoloChemReactions.{hh,cc}` (local `kOH`/`kHO2`/`Conf()`, portable, `ChemistryTypes.hh`-only dependency). `git diff -- src/chemistry/BuiltInChemistries.cc` shows exactly the specified 2-argument registration plus its include. `git diff -- CLAUDE.md` shows exactly one new Key Files bullet (right after the `Tonneau2025Reactions.cc` bullet, same style) plus the `BuiltInChemistries.cc` bullet's chemistry list updated to include `Farokhi2023`. No changes made after review.

## Blockers / Challenges

None in the implementation itself. Subagent's first build attempt (plain Bash `cmake --build`, no MSVC env loaded) failed with a missing `<atomic>` header — expected per `.claude/geant4-instructions.md` section 1, not a code defect; it re-ran via PowerShell with vcvars64.bat loaded and the build succeeded.

## Commits

- 2f38463 feat: add Farokhi2023 Chemistry (ticket 03)

## Time Spent

6m (ticket-start.js to ticket-complete.js)
