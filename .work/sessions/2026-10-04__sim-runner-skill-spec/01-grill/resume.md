# Session: sim-runner-skill-spec

**Date:** 2026-10-04T08:32:46.598Z
**Status:** Grill phase complete

## Problem Statement

Geant4 simulations (first user: dnachem-min `sim.exe`) are started by hand or by ad hoc PowerShell scripts (`macro/test_run.ps1`). Nothing records what is running, what ran (with which binary, macro and parameters) or what should run next, and analysis scripts are invoked by hand on output directories. This session produces a specification (`~/dotfiles/claude/skills/g4run/SPEC.md`) detailed enough to build a global Claude skill, `/g4run`, that queues, launches, tracks and analyses Geant4 simulation runs through a SQLite database. Building the skill itself is a later session.

## Context & Constraints

- **Current behavior:** runs are launched manually from `build/` (`./sim <macro> [--threads N] [--dir <path>]`, `sim.cc` prepends `macro/` to the argument); sweeps are scripted ad hoc (`macro/test_run.ps1` loops over O2 levels); analysis scripts (e.g. `analysis/compare_reference.py`, conda env `GEANT4_py311`) are run by hand on dump directories. No record of run history, binary provenance or planned runs.
- **Pain point:** no single view of running/finished/planned runs; rebuilding `sim.exe` silently changes what later runs use; no resource control when several runs share the machine; analysis results are not traceable to the runs they used.
- **Dependencies:** global conventions in `~/dotfiles/claude/COMMON_CLAUDE.md` (paths only via `${DEV_DIR}` / `${G4_ROOT}`, never hardcoded; project settings in `.claude/.claude-project.json`, resolved per `~/.claude/CONFIG_RESOLUTION.md`). The existing `run` block of `.claude-project.json` (`build.executable`, `run.workingDir`, `run.macroDir`, `run.macroArgIsFilenameOnly`, `run.cli`, `run.successMarkers`, `run.failureMarkers`) must be reused, extended by a new `g4run` block, not duplicated. sim output semantics per the `sim-output` skill (output only at `/run/dumpDataAndReset` or exit; `Manifest.json` per dump).
- **Tech stack:** Python CLI invoked by `SKILL.md` (same pattern as gps's node scripts); SQLite (stdlib `sqlite3`, WAL mode); psutil for process/memory tracking; a dedicated venv created by the skill on first use (base interpreter: miniconda base Python); Windows 11 (toasts, process-tree kill, detached processes).

## Success Metrics

- `SPEC.md` exists at `~/dotfiles/claude/skills/g4run/SPEC.md`, committed in the dotfiles repo, and dnachem-min holds a pointer to it.
- The spec covers every decision of this grill: scope/config, storage layout, DB schema (tables, columns, states), enqueue/sweep/macro templating, binary snapshot, dispatcher (lifecycle, budget, memory estimate, free-RAM check, scheduling), run wrapper, failure handling, cancel, prune, notifications, analysis contract and registration, subcommand reference, and the `.claude-project.json` `g4run` block.
- An implementer can build the skill from the spec alone without guessing a design decision (each remaining choice is listed under open questions, not left implicit).

## Architecture & Approach

**Skill:** global, `/g4run`, in `~/dotfiles/claude/skills/g4run/` (symlinked into `~/.claude/skills`). Config-driven: per-project settings come from `.claude/.claude-project.json` (existing `build`/`run` fields + a new `g4run` block: memory default, analysis registrations). No hardcoded paths: everything derives from `${DEV_DIR}` / `${G4_ROOT}`.

**Storage (runs root):** `${DEV_DIR}/GEANT4/RUNS/` (overridable globally) holds the single global SQLite DB (project column, WAL), the skill's venv, and one dedicated directory per run: `<project>/<runId>/` containing the copied executable + DLLs from the run build dir (snapshot taken at enqueue), `macro/<rendered>.in`, `output/` (passed as `--dir`), `stdout.log`. Provenance in DB: exe hash, git commit, dirty flag, full rendered macro text, parameters. Run ID: `YYYYMMDD-HHMMSS-<macro>-<4hex>`.

**Enqueue:** an existing macro file with optional `{{key}}` placeholders filled by `--set k=v` or `--sweep k=a,b,c` (cartesian, one run per combination). Options: `--threads`, `--mem` (override), `--at <datetime>` (not-before), `--tag`, batch name/note. Each enqueue creates a **Batch**; runs and batches carry free tags. Selectors everywhere: run id, batch id/name, `--tag`, `--project`, `--status`.

**Dispatcher:** detached, single-instance (lock); auto-revived by any `/g4run` subcommand; sleeps until a slot frees or the next not-before time; exits when no queued run remains. After a reboot nothing runs until the next command; `status` warns about overdue runs. Admission requires all of: thread budget (sum of running `--threads`, Serial = 1, + candidate ≤ budget; default physical cores − 1, settable globally); memory estimate fits (learned: max peak RSS of past finished runs of the same project + macro, scaled per thread; fallback project default in `g4run` config; `--mem` overrides); free-RAM check (system free RAM above a safety margin at launch).

**Run wrapper:** each run is launched through a small Python wrapper process that starts the executable in the run dir, records PID, samples peak RSS, captures stdout to `stdout.log`, records exit code and classifies success via exit code + `successMarkers` / `failureMarkers`. States: `queued → running → succeeded | failed | cancelled | lost`, plus `pruned`. Reconciliation: a `running` row whose wrapper is no longer alive (PID + create time) becomes `lost`.

**Failures:** no auto-retry. `requeue <selector>` creates a new run (new id, fresh dir, link to the original); history is never rewritten.

**Cancel:** queued → `cancelled` immediately; running → Claude confirms via AskUserQuestion, then kills the process tree; state `cancelled`, partial output kept.

**Prune:** manual only. `prune <selector>` (e.g. `--status failed --older 30d`) shows sizes, asks confirmation, deletes run dirs; DB rows stay, marked `pruned`. `--binaries` removes only copied exe/DLLs from finished runs.

**Notifications:** Windows toast from the dispatcher when a batch completes (all runs terminal) or any run fails / is lost.

**Analysis:** generic scripts shipped with the skill (timing/memory/log stats, manifest table) + project-specific scripts kept in the project's git and registered in `.claude-project.json` under `g4run.analysis.<name> {script, args, env}` (e.g. `compare_reference.py` in `GEANT4_py311`). Contract: script receives `--runs <json>` (per run: id, params, output dir, manifest path) and `--out <dir>`; writes files there plus optional `summary.json`. Each invocation is an `analyses` DB row (script, script git hash, input runs, out dir, exit code). Subcommands: `analyze <name> <selector>`, `analyses`.

**Subcommands:** `enqueue`, `status`, `show`, `log`, `cancel`, `requeue`, `prune`, `analyze`, `analyses`, `dispatcher status|stop`.

## Assumptions & Trade-offs

- Global DB (not per project): one view of everything on the machine, needed for a machine-wide thread/memory budget; the trade-off is that history doesn't travel with a repo.
- Per-run copies of exe + DLLs (not a content-addressed store): simple and self-contained run dirs, at the cost of ~1–2 MB per run, reclaimable with `prune --binaries`.
- Dispatcher is not a Windows service / logon task: scheduled runs don't start after a reboot until the next `/g4run` command.
- No auto-retry, no auto-pruning, no per-run toasts.
- No scalar results table: analysis results are files + a logged invocation, not queryable values.
- Not doing: building the skill (later session), remote/cluster execution, UHDR pulse structure, any change to dnachem-min C++ code.
- Assumes the executable's DLLs that are not in the run build dir (Geant4, Qt) are found via PATH, as for a manual run.

## Open Questions

- Exact free-RAM safety margin and per-thread memory scaling formula (spec should propose defaults: e.g. 15 % free RAM; estimate = base + per-thread × N fit from history, or peak × N / N_hist).
- Toast mechanism on Windows without extra dependencies (PowerShell `BurntToast` vs. WinRT via PowerShell vs. a pip package in the venv): spec to pick one.
- Whether the run's `macro/` dir must also contain macros included by the main macro (`/control/execute sub_run.mac`): spec should copy the whole project `macro/` dir and render only the main one.

## Notes

- Decisions taken during the grill (all by the user): global config-driven skill; one global DB; dispatcher + time scheduling; thread budget + learned memory estimate + free-RAM check; existing macro + `{{}}` sweeps, snapshotted; binary snapshot at enqueue into a dedicated run directory (exe + DLLs directly there); global runs root `${DEV_DIR}/GEANT4/RUNS`; no auto-retry; detached auto-revived dispatcher; analysis = generic global + project-registered scripts; JSON-in/files-out contract logged in DB; batch + tags; dedicated venv; name `/g4run`; cancel kills with confirmation; manual prune only; toast per batch; spec in dotfiles.
- Defaults chosen by Claude, accepted with the design: run-id format, `{{key}}` placeholder syntax, Python CLI called from SKILL.md, no glossary/ADR entries in dnachem-min (tooling outside its domain).
- Facts checked: `build/` holds `sim.exe` (1.3 MB) + `hdf5.dll`, `hdf5_cpp.dll`; base Python 3.14 lacks psutil; `GEANT4_py311` has psutil 7.2.2 + pandas; `sqlite3` CLI available in miniconda.
