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
- Orket (the local scheduler): `g4-study.ps1 submit-run` is the asynchronous twin of
  `run`. It writes the stamp (status `submitted`), queues one orket job and returns.
  `g4-study.ps1 sync <study>` completes the stamps once the jobs finish. Both runners
  take `-Orket` with `-Study`:
  - `examples/dose-rate-study/studies/run_study.ps1 -Study <s> -Orket` submits each
    history of a plan. It refuses a wall-time cap: pass `-WallTimeCapS 0` when the
    plan sets one. It writes no `run_log.csv`.
  - `test/dose-rate-patches/RunNecessity.ps1 -Study <s> -Orket` submits each history,
    JobLauncher caps included, then stops. After `sync`, rerun it with `-Study -Reuse`
    and no `-Orket` to link the outputs and analyze.

  Both skip runs already submitted or succeeded. The orket jobs need the machine's one
  `g4-study` simulator, registered once with `g4-study.ps1 orket-profile` and the
  `orketctl` command it prints. The orket daemon starts jobs with its own environment,
  so `GEANT4_DATA_DIR` must be set for it. The Geant4 DLLs travel in each run's Binary
  snapshot, taken from the bin dir on PATH at submission.

The earlier `/g4run` design (queue, dispatcher, SQLite; specified in
`~/dotfiles/claude/skills/g4run/SPEC.md`, never built) is retired: see
`docs/adr/0012-study-tooling-replaces-g4run.md`.
