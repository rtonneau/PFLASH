# Simulation studies (`/study` skill)

`/study` is a global Claude skill (in the dotfiles repo, not in this repo) that records
simulation studies: every run whose result is used goes through its wrapper, which
writes a provenance stamp beside the output. The stamp holds the command line, macro,
seeds, git commit and diff, exe hashes, Geant4 install and patch set, machine and
outcome.

- Skill: `~/dotfiles/claude/skills/study/SKILL.md` (installed as `~/.claude/skills/study`
  by `claude-sync-skills.ps1`). The wrapper is `scripts/study.ps1`; run `study.ps1 help`.
- Data: `${STUDIES_ROOT}/PFLASH/<studyId>/`, outside git. `STUDIES_ROOT` is a
  user env var set by `~/dotfiles/scripts/windows/setup-dev-env.ps1 -StudiesRoot <path>`.
- This repo: the `studies` block of `.claude/.claude-project.json` names the project and
  the committed Study index, `docs/studies/INDEX.md`. Reports stay in `docs/<topic>/`
  with only their final figures.
- Geant4 installs carry a `g4-build-info.json` marker (patch set, DLL hashes):
  `~/dotfiles/claude/skills/study/references/g4-build-info.md`.

The earlier `/g4run` design (queue, dispatcher, SQLite; specified in
`~/dotfiles/claude/skills/g4run/SPEC.md`, never built) is retired: see
`docs/adr/0012-study-tooling-replaces-g4run.md`.
