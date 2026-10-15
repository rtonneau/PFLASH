# Session retro — 2026-10-09

Reviewed the 15 most recent Claude Code sessions on this project (session transcripts under
`~/.claude/projects/c--DEV-GEANT4-SIM-dnachem-min/`, analyzed by three background agents split
by file size). Findings below, ordered by severity, per the `/retro` skill's categories
(navigation, automated checks, coding standards, CLAUDE.md bloat, tool economy, information
access).

## High severity

1. **Git-tracked files that should be ignored.** `.claude/hooks/hook-posttooluse.log`
   (rewritten on every tool call) and `compile_commands.json` are both tracked in git despite
   matching `.gitignore` patterns — added before the ignore rule existed. Shows up as noise in
   nearly every `git diff`/`status`, forced a `git stash -u` detour in one session, and actually
   **blocked a `git switch main`** in another ("local changes would be overwritten"). Fix:
   `git rm --cached` both files once.

2. **Output-file normalization formulas aren't written down anywhere.** The user asked
   "remind me, is `Reactions_nt_reactions.csv` sum or average?" then immediately asked the same
   kind of question about `Species_nt_species.csv` — "remind me" implies this has come up
   before. The agent had to re-derive sum-vs-mean/error-bar formulas live from
   `ScoreSpecies.cc` each time. `docs/output/README.md` and the `sim-output` skill list the
   columns but not the normalization math. Fix: write the formula once into one of those.

3. **A behavior-affecting flag got a config-presence test instead of an effect test.** The
   SBS max-time-step-cap test only asserted the value appears in `Manifest.json`, never that
   actual step sizes are bounded — and the agent's own later diagnosis named this as the likely
   cause of a downstream ticket failing. Mechanical pattern worth a standing rule: "a test for a
   flag must assert its runtime effect, not just that it was recorded" — reviewer checklist line
   or `CODING_STANDARDS.md` entry.

## Medium severity

4. **Geant4 source navigation guess was wrong.** The agent assumed
   `G4ChemDissociationChannels_option1.cc`-type physics-list constructors live under
   `dna/utils/src/`; they're actually under `physics_lists/constructors/electromagnetic/src/`.
   Cost: 3 extra tool calls. A one-line KB/CLAUDE.md pointer distinguishing "EM-DNA physics-list
   constructors" from "molecule/management source" would prevent this.

5. **No committed convention for waiting on a background sim run.**
   `.claude/geant4-instructions.md` says to background it and read `run.log`, but gives no
   expected duration or single waiting mechanism — one session produced ~8 turns of the agent
   re-deciding how to wait on a smoke test.

6. **"Verify against Geant4 source" is applied only at review, not while drafting.** A plan
   named an SBS constructor that doesn't exist in 11.4.1; it was caught and fixed, but only
   during plan review, not while writing the plan. Worth a plan-review checklist line: "every
   named Geant4 class/method confirmed via grep in `geant4SourcePath`."

7. **F-003 (concurrent `/chem/reaction/dump` writes across MT threads) is an unverified
   concurrency gap** with no test or lint guarding it — it was dropped as a backlog idea without
   confirmation it's actually fixed.

## Low severity

8. PDF/figure tooling gaps (`pdftoppm`, `fitz`/PyMuPDF) caused repeated failed attempts in two
   separate sessions when digitizing a figure from a paper; worth a one-line note on what's
   actually installed (and that PyMuPDF imports as `pymupdf`, not `fitz`).
9. Git-Bash on Windows rejects `tail -3` (needs `tail -n 3`) — this made a background task
   report "failed" even though the simulation run itself succeeded.
10. The Bash tool's persistent cwd caused `rm -rf build/smoketest` to fail with "Device or
    resource busy" after an earlier `cd` into that directory.

## Positive controls (no fix needed)

- The `RunManifest`/`RunAction` navigation pointers in CLAUDE.md led straight to the right file
  with zero wasted search in one session.
- A domain-vocabulary collision ("reaction model" vs "time step") was proactively caught and
  fixed in `CONTEXT.md` mid-session rather than causing confusion later.
