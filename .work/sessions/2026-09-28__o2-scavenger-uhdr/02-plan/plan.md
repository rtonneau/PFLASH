# Implementation Plan

**Session:** o2-scavenger-uhdr
**Date:** 2026-09-28T20:09:36.710Z
**Estimated effort:** 1 day

## Strategy

Implement dissolved O2 as a bulk scavenger the way the Geant4-DNA UHDR example does, and replace the no-op `/chem/env/O2` with a generic `/chem/env/scavenger <species> <value> <unit>`. The scavenger concentration belongs to the environment: `DnaChemistryWorld` stores it and puts it into `fpChemicalComponent`. The reactions against it belong to the selected Chemistry: three `O2` partner lines go into the per-molecule bulk-reaction list (today's acid-base list, renamed). A kernel-free `ScavengerSpec` module does the parsing, unit conversion and checks, and a unit test covers it. The full step-by-step plan, with the code for every step, is in `.claude/plans/2026-09-28-o2-scavenger-uhdr.md`. Each ticket follows the task of the same number there.

Shared procedures (full text in the plan file):
- Builds run inside the MSVC `vcvars64.bat` wrapper (PowerShell tool): `sim` in `build/` (RelWithDebInfo), tests in `build-ninja/` (Debug): `cmake --build build-ninja && ctest --test-dir build-ninja --output-on-failure`. All Passed, none Not Run.
- Smoke macro `build/macro/scav_smoke.in` (scratch, untracked): 10 keV e-, `/run/beamOn 2`, `/dnaLogger/verbose Warning`, `/chem/reaction/dump reactions_dump.txt` before `/run/initialize`, `/run/dumpDataAndReset` at the end, plus an optional `<SCAVENGER_LINE>`. Run it from `build/` in the background with `--dir smoke/<name>`.
- Deterministic comparison: the sorted `reactions_dump.txt`, plus `EnergyDeposit.Txt` and `PhysicsInteractions.csv`, which are byte-identical thanks to the fixed seed. Species yields are not reproducible, so compare them for plausibility and direction only.

## Tickets Overview

- **Ticket 1:** Rename acid-base list → bulk-reaction list throughout (types, builders, registry field, registration method, dump section). No behavior change. Commits the grill docs (ADR 0004, ADR 0001 note, CONTEXT.md).
- **Ticket 2:** Add the pure-logic `ScavengerSpec` module (parse, unit→molarity, % O2-only, pH-owned rejection, last-wins upsert, inert-species check) and `test/ScavengerSpecTest.cc`.
- **Ticket 3:** Add the `/chem/env/scavenger` command (`ScavengerMessenger`), move `DnaChemistryWorld` to a scavenger list, add the unknown-species fatal error and the inert-scavenger warning, remove `/chem/env/O2`, migrate the macros, update CLAUDE.md.
- **Ticket 4:** Add the three O2(B) bulk reactions (UHDR rates) to PureWater and BoscoloChem, run the acceptance matrix (PureWater only), and document the absorbed-O2-products consequence.

## Sequencing Rationale

Ticket 01 is a pure rename, checked against a baseline captured before the first edit. Doing it first means every later ticket uses the final names. Ticket 02 adds kernel-free code that nothing calls yet, so `sim` stays unchanged. Ticket 03 is the first user-visible change (command swap). It still leaves the chemistry unchanged at 0 %, which the regression check shows. Ticket 04 adds the reactions last, so the "inert" warning from ticket 03 serves as its failing check, and it runs the full acceptance matrix once.

## Risks & Mitigation

- **Risk:** While bulk O2 > 0, Geant4 sends every `O2` product (of tracked and bulk reactions) into the bulk pool (`G4DNAScavengerProcess::PostStepDoIt`, `G4DNAMakeReaction`), so tracked O2 disappears from `Species.Txt` at 21 % → **Mitigation:** this is the same as UHDR and expected. Ticket 04 records the observed G(O2) and documents it in ADR 0004 and CLAUDE.md.
- **Risk:** Unrelated uncommitted edits in `CLAUDE.md` (the move to the `sim-output` skill) and the untracked `.claude/skills/` could be swept into ticket commits → **Mitigation:** commit them separately before ticket 01. Tickets stage explicit paths only.
- **Risk:** A 2-event direction check can be noisy → **Mitigation:** if G(e_aq)/G(H) do not clearly drop, rerun with `/run/beamOn 10` after confirming that the dump shows the O2(B) lines.
- **Risk:** `G4UIcommand` joins the three parameters into one `newValue` string that `ScavengerSpec::Parse` tokenizes again → **Mitigation:** the ticket 03 scratch macros exercise the real command path.

## Assumptions

- `build/` and `build-ninja/` are already configured (incremental builds only).
- The ADR 0004 text written during grill counts as accepted. Ticket 01 commits it, and ticket 04 adds only the absorbed-O2 consequence.
- BoscoloChem gets exactly the same code changes as PureWater but is never run in a smoke test (user decision). A successful build is its only check.
- Species names are case-sensitive, as in `G4MoleculeTable`.

## Token Usage

- **Input:** 62
- **Output:** 47216
- **Cache read:** 3958767
- **Cache creation:** 123596
- **Total:** 4129641
