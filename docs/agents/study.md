# Simulation studies (`/g4-study` skill)

`/g4-study` is a global Claude skill (in the dotfiles repo, not in this repo) that records
simulation studies: every run whose result is used goes through its wrapper, which
writes a provenance stamp beside the output. The stamp holds the command line, macro,
seeds, git commit and diff, exe hashes, Geant4 install and patch set, machine and
outcome.

- Skill: `~/dotfiles/claude/skills/g4-study/SKILL.md` (installed as `~/.claude/skills/g4-study`
  by `claude-sync-skills.ps1`). The wrapper is `scripts/g4-study.ps1`; run `g4-study.ps1 help`.
  It was named `/study` (`study.ps1`) until 2026-11-19.
- Data: `${STUDIES_ROOT}/PFLASH/<studyId>/`, outside git. `STUDIES_ROOT` is a
  user env var set by `~/dotfiles/scripts/windows/setup-dev-env.ps1 -StudiesRoot <path>`.
- This repo: the `studies` block of `.claude/.claude-project.json` names the project and
  the committed Study index, `docs/studies/INDEX.md`. Reports stay in `docs/<topic>/`
  with only their final figures.
- Geant4 installs carry a `g4-build-info.json` marker (patch set, DLL hashes):
  `~/dotfiles/claude/skills/g4-study/references/g4-build-info.md`. `g4-study run`
  refuses an install without one (Unmarked) or whose DLLs changed since marking
  (Stale), unless `--allow-unmarked`: mark an install with `g4-study.ps1 mark` before
  its Study runs (the `.scratch/kernels` installs included).
- The wrapper serializes parallel starts itself (Study and project locks), and
  `g4-study.ps1 check` exits 1 when `docs/studies/INDEX.md` and the `study.json`
  files disagree: change rows with `g4-study.ps1 close`, never by hand.

The earlier `/g4run` design (queue, dispatcher, SQLite; specified in
`~/dotfiles/claude/skills/g4run/SPEC.md`, never built) is retired: see
`docs/adr/0012-study-tooling-replaces-g4run.md`.
