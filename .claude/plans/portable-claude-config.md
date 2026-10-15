# Plan: portable Claude Code config across Windows desktops

Status: **v6 — EXECUTED on this machine (2026-09-24); committed locally in 3 repos, nothing pushed.** See "Execution log" at the end for what was built, what was verified, deviations from this plan, and follow-ups.
Date: 2026-09-24

## Goal

Make Claude Code work on several Windows desktops where the **DEV root**
differs (e.g. `C:\DEV` vs `D:\DEV`) and the Geant4 install may live
elsewhere — while keeping `~/.claude` a normal, per-machine directory.

## Decisions (user)

1. Two user-level env vars per machine:
   - **`DEV_DIR`** (e.g. `C:\DEV`) — layout below it is identical everywhere.
   - **`G4_ROOT`** = the **Geant4 install dir**
     (today `C:\DEV\GEANT4\geant4-v11.4.1-install`); may be on another drive.
   Dev projects live under `${DEV_DIR}/GEANT4/SIM/`. Tracked config never
   contains a drive letter.
2. **No `CMakeLists.txt` is modified** (any project, incl. the template).
3. The dotfiles repo (`~/dotfiles`) is the **only** place that validates
   the two vars. Projects consume only `$env:DEV_DIR` / `$env:G4_ROOT`;
   they never call into `~/.claude` or `~/dotfiles`.
4. **Minimal dotfiles footprint.** `~/.claude` stays a real per-machine
   directory. Dotfiles hold only:
   - the **hooks** (linked to `~/.claude/hooks/` by `bootstrap.ps1`),
   - a **`COMMON_CLAUDE.md`**, which `bootstrap.ps1` makes
     `~/.claude/CLAUDE.md` import (adds the import line if missing),
   - a **`common_settings.json`**: the settings every machine should have.
     `bootstrap.ps1` runs a command that **compares** it to
     `~/.claude/settings.json`, **shows what is missing**, and **asks
     before** adding it. `~/.claude/settings.json` stays a real local file.
   Everything else in `~/.claude` (`commands/`, `skills/`,
   `CONFIG_RESOLUTION.md`, `KNOWLEDGE_BASE_BUILDER.md`, plugins, credentials,
   memory, and any setting not in `common_settings.json`) stays **local and
   unsynced**.

## Path map (single reference; replaces every hardcoded path)

| Item | Expression |
|---|---|
| `geant4InstallPath` (`Geant4_DIR` = `<it>/lib/cmake/Geant4`) | `${G4_ROOT}` |
| `geant4SourcePath` | `${DEV_DIR}/GEANT4/geant4-v11.4.1-source/geant4-v11.4.1` |
| `kbPath` | `${DEV_DIR}/GEANT4/geant4-v11.4.1-kb` |
| G4Vox (`G4Vox_DIR` = `<it>/lib/cmake/G4Vox`) | `${DEV_DIR}/GEANT4/LIB/G4Vox-install` |
| `build.toolchainFile` | `${DEV_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake` |
| template repo (`g4-new`) | `${DEV_DIR}/GEANT4/SIM/geant4-dna-template` |
| projects | `${DEV_DIR}/GEANT4/SIM/<project>` |
| `additionalDirectories` | source tree, its `examples/`, KB (all `${DEV_DIR}/GEANT4/...`) |

## Verified facts

Docs (2026-09-24):
- **Hooks:** `command` is shell-expanded (Git Bash `$DEV_DIR`, PowerShell
  `$env:DEV_DIR`); with `args` (exec form) no shell expansion, only
  placeholders like `${CLAUDE_PROJECT_DIR}`. Hooks inherit Claude's env.
- **Permission paths:** `//abs`, `~/`, `/rel-to-settings`, `./rel`.
  **Env-var expansion not documented** → untested (Step 0). On Windows
  paths are normalised to POSIX; `//**/.env` documented as "all drives".
- `~/.claude/settings.local.json` is only read for sessions started in the
  home directory → can't carry user-wide per-machine rules.
- **CLAUDE.md imports:** `@path` syntax; relative or absolute paths;
  relative paths resolve against the importing file; `@~/...` is used in the
  docs' own example; max depth 4; **imports inside code spans / fenced code
  blocks are not parsed**; imported files load at launch (count toward
  context; keep CLAUDE.md files < 200 lines). Imports in **user-scope**
  `~/.claude/CLAUDE.md` load **without an approval dialog**.
  **Caveat:** in Claude Desktop *Cowork* sessions, user-scope imports that
  resolve outside the session's working directory are **skipped** (and a
  symlinked `~/.claude/CLAUDE.md` is skipped) → the import would not load
  there; CLI/IDE sessions are unaffected. Also, Edit/Write refuse to write
  through a symlink → `~/.claude/CLAUDE.md` must stay a real file (it does:
  we import, we don't link it).
- Imported content is expanded where the `@` line sits → a line at the top
  loads first, a line at the end loads last.

Dotfiles repo (`~/dotfiles`, `git@github.com:rtonneau/dotfiles.git`,
**private**, `main`):
- `bootstrap.ps1` (Windows; `Link` = `New-Item -ItemType SymbolicLink -Force`,
  `Add-ToPath` sets a *User* env var), `bootstrap.sh`, `scripts/` (on
  PATH), `git/`, `ssh/`, `shell/`, `.vscode/`, `.github/`, `geant4/`.
- No `claude/` dir yet; no `DEV_DIR` / `G4_ROOT` anywhere.
- 2 unrelated uncommitted changes (`.vscode/settings.json`,
  `git/.gitconfig`) — must not be swept into our commits.
- `geant4/CMakeUserPresets_AT_HOME.json` hardcodes `C:/DEV/vcpkg` and
  `E:/GEANT4/...` — consistent with `G4_ROOT` independent of `DEV_DIR`.

This machine (before Step 0): `DEV_DIR` unset; **`G4_ROOT` was already set**
at User scope to `C:\DEV\GEANT4\geant4-v11.4.1-install` (the intended value;
earlier drafts wrongly said "both unset"). `~/.claude/hooks/` is a real dir holding only
`plugins-version.sh`. Project hardcoded paths live only in
`.claude/settings.json`, `.claude/.claude-project.json`,
`.claude/hooks/update-compile-cmd.ps1`, `~/.claude/commands/g4-new.md`.

## Steps

### 0. Prerequisite + empirical checks (scratch only, revert after)
- User-scope `setx DEV_DIR C:\DEV` and
  `setx G4_ROOT C:\DEV\GEANT4\geant4-v11.4.1-install`; restart
  terminal/IDE/Claude.
- (a) Does `${DEV_DIR}/GEANT4/...` work in `additionalDirectories`?
- (b) Does `Edit(//**/geant4-v11.4.1-source/**)` deny on any drive? (would
  let the tracked project `settings.json` carry the deny with no drive
  letter.)
- (c) Does a hook run correctly through a **symlinked `~/.claude/hooks/`**
  (SessionStart fires, script executes, JSON output shown)?
- (d) Sentinel test: a scratch `@~/dotfiles/...md` line at the top of
  `~/.claude/CLAUDE.md` is loaded (ask something only the sentinel answers).
- (e) `pwsh --version` ≥ 7 on this machine (needed by Step 1.7).
- Outcomes select 2a/2b in Step 2 and confirm Steps 1.4–1.5.

#### Step 0 results (2026-09-24)
Done: `DEV_DIR=C:\DEV` set at User scope (`G4_ROOT` was already correct,
left untouched). All target dirs exist (install, `Geant4Config.cmake`,
vcpkg toolchain, `SIM`, source, KB, `LIB\G4Vox-install`). Tests were run
headless (`claude -p`, `--setting-sources project,local`, `--tools
Read,Edit`) in two throwaway scratch projects under `%TEMP%` (now deleted,
with their empty `~/.claude/projects` entries), judged on **filesystem
state**, not on the model's report:

| Check | Result | Consequence |
|---|---|---|
| (a) `${DEV_DIR}/...` in `additionalDirectories` | **Not expanded** — the read was refused; the identical literal path (control) was allowed | **2b is the design**: per-machine generated `settings.local.json` |
| (b) `Edit(//**/geant4-v11.4.1-source/**)` deny | **Works** — edit refused, sibling `ok/` edit allowed; tested on `C:` under `%TEMP%` (drive-independence rests on the docs' `//**` = all drives) | Tracked project `settings.json` carries this deny, no drive letter |
| (c) hook script reached through a **junction** | **Works** (marker file written) | Link `hooks/` with a junction |
| symlink creation (non-elevated) | **Fails**: "Administrator privilege required" (Developer Mode off; the existing `~/.bashrc` etc. links were made by an elevated run) | `[Claude]` uses **junctions** → no admin needed for the new section |
| junction creation (non-elevated) | Works | — |
| (d) `CLAUDE.md` import sentinel | **Deferred** — needs the real user-scope `~/.claude/CLAUDE.md`; runs in Step 1 verification | — |
| (e) PowerShell | 7.6.6 ✓ | Step 1.7 can require pwsh 7 |

Also learned:
- Recursive delete of a directory containing a junction **refuses to
  proceed** ("access denied on `hooklink`"). Unlinking must be
  **non-recursive** (`[IO.Directory]::Delete(<link>)`), junction first;
  bootstrap must never `Remove-Item -Recurse` / recursive-delete a link.
- `~/.claude.json` contains keys differing only by case
  (`c:/DEV/...` vs `C:/DEV/...`), so plain `ConvertFrom-Json` **throws**
  on it. `sync-claude-settings.ps1` therefore parses with
  `-AsHashtable` (and must be tested for key-order preservation), even
  though today's `settings.json` has no such keys.

### 1. Dotfiles repo (branch `claude-config`; commit only files of this work)
1. `claude/hooks/plugins-version.sh` — moved from `~/.claude/hooks/`.
2. `claude/hooks/check-env.ps1` — **single validator**, the only place that
   knows what valid values are:
   - `DEV_DIR`: set; existing dir; contains
     `vcpkg/scripts/buildsystems/vcpkg.cmake` and `GEANT4/SIM`.
   - `G4_ROOT`: set; existing dir; contains
     `lib/cmake/Geant4/Geant4Config.cmake`.
   - **Version coupling:** if the `G4_ROOT` leaf is `geant4-v<X>-install`,
     require `${DEV_DIR}/GEANT4/geant4-v<X>-source` to exist; a leaf that
     doesn't match the pattern → warning, not error.
   - Var set at User scope but absent from the process → "restart your
     shell/IDE" instead of "unset".
   - Non-zero exit on failure; `-Hook` switch prints the
     `{"systemMessage": ...}` JSON for hooks.
   Called from three places, none re-implementing it: the `SessionStart`
   hook (Step 1.6), `bootstrap.ps1 [Env]`, `init-claude-project.ps1`.
3. `claude/COMMON_CLAUDE.md` — shared instructions (< 200 lines).
   Initial content (decided): **(a)** the current global
   "Global Geant4-DNA Conventions" text from `~/.claude/CLAUDE.md`, moved
   as-is; **(b)** a new **Path variables** rule — `${DEV_DIR}` /
   `${G4_ROOT}` in any `.claude-project.json` value are replaced by the
   env var before use; unset → STOP and tell the user, never guess — plus
   the path map above. (Lives here, not in the local
   `CONFIG_RESOLUTION.md`, because it must sync.)
   **Dependency (decided: warn only, Step 1.10):** (a) points at
   `~/.claude/CONFIG_RESOLUTION.md`, `~/.claude/KNOWLEDGE_BASE_BUILDER.md`
   and `~/.claude/commands/kb-*`, which are *not* in dotfiles.
   **Duplicate-content hazard:** after the import is added, the same
   conventions in the local `CLAUDE.md` would load twice → Step 3 removes
   them locally; bootstrap **warns** (never edits) if the local file still
   contains the `# Global Geant4-DNA Conventions` heading.
4. `bootstrap.ps1` **`[Claude]` → hooks:** **junction** (`New-Item
   -ItemType Junction`, no admin — Step 0) `~/.claude/hooks` →
   `dotfiles\claude\hooks`. Never overwrite content: absent → link;
   real empty dir → replace by link; real dir with files not in dotfiles →
   stop and print what to move; already the right junction → `[SKIP]`;
   a junction to somewhere else → stop and print it. Replacing an empty
   dir or removing a link is always non-recursive.
5. `bootstrap.ps1` **`[Claude]` → CLAUDE.md import:**
   - Import line: `@~/dotfiles/claude/COMMON_CLAUDE.md` (no drive letter;
     dotfiles path is already fixed at `$HOME\dotfiles` by bootstrap).
   - `~/.claude/CLAUDE.md` missing → create it with just that line.
   - Present → look for a **non-code-fenced** line whose import path ends
     with `COMMON_CLAUDE.md` (a hit inside a code block/backticks doesn't
     count: it isn't an import). Found → `[SKIP]` (warn if its target
     doesn't exist).
   - Not found → **insert at the very top** (recommended: shared baseline
     loads first, machine-local text below can refine it), followed by a
     blank line. Back up first to `~/.claude/backups/CLAUDE.md.<timestamp>.bak`;
     keep the file's line endings and encoding; idempotent; supports
     `-WhatIf`.
6. **`claude/common_settings.json`** — strict JSON (no comments), same
   schema as `settings.json`, containing only what must exist everywhere.
   Initial content (proposed): the two `SessionStart` (`matcher: startup`)
   hooks — `~/.claude/hooks/plugins-version.sh` and
   `powershell.exe -NoProfile -ExecutionPolicy Bypass -File
   "$HOME/.claude/hooks/check-env.ps1" -Hook` (linked scripts don't run
   unless registered, and `settings.json` is local). **Decided: hooks
   only for now.**
7. **`scripts/sync-claude-settings.ps1`** (on PATH; runnable standalone,
   called by `bootstrap.ps1 [Claude]`; `#Requires -Version 7` for reliable
   JSON handling). Behaviour:
   - **Compare** `common_settings.json` → `~/.claude/settings.json`:
     - objects: recurse; a key absent locally is *missing*;
     - arrays of scalars (e.g. `permissions.allow`): missing elements =
       set difference;
     - arrays of objects: matched by deep equality, except **hooks**,
       identified by (event, matcher, hook `command`) so an entry already
       registered under a differently-shaped group isn't duplicated;
     - a scalar that exists locally with a **different** value is a
       *conflict*: it is listed with both values and **offered for
       overwrite item by item** (`Overwrite <path> (local: X → common: Y)?
       [y/N]`, default no) — never overwritten without that per-item yes;
     - settings only present locally are ignored.
   - **Show** the result as a readable list (`+ hooks.SessionStart[startup]
     → command …`, `~ model differs (local: …, common: …)`); nothing
     missing or conflicting → "in sync", exit 0, no prompt.
   - **Ask** (1) `Add N missing item(s) to ~/.claude/settings.json? [y/N]`
     (default no), then (2) one prompt **per conflict**. The two are
     independent: you can add the missing items and decline every
     overwrite, or the reverse. Non-interactive: `-Yes` adds missing items
     **only**; overwriting additionally requires `-Overwrite`; `-Check`
     only reports (exit 1 if anything is missing or conflicting); with no
     switch and no TTY it never blocks or writes.
   - **Apply** (only on yes): back up to
     `~/.claude/backups/settings.json.<timestamp>.bak`; change only the
     accepted items; preserve key order and all other content; re-serialise
     with depth ≥ 20 and re-parse to validate before replacing the file
     (write to a temp file, then move). Local file missing → offer to
     create it from `common_settings.json`. Local file invalid JSON →
     abort with the parse error, write nothing.
8. `bootstrap.ps1` **`[Env]`**: prompt/`-DevDir`/`-G4Root`, set User vars,
   run `check-env.ps1`, "restart your shell" note; bootstrap fails if the
   check fails. `[Claude]` then calls `sync-claude-settings.ps1` (Step 1.7)
   after the hooks link (Step 1.4) so the registered hooks point at files
   that exist.
9. `scripts/init-claude-project.ps1 [project-dir]` (same convention as
   `bootstrap-prompts.ps1`): runs `check-env.ps1` first and aborts on
   failure; warns if the project isn't under `${DEV_DIR}/GEANT4/SIM`;
   generates `<project>/.claude/settings.local.json` with the machine's
   `additionalDirectories` (2b only); idempotent, no overwrite without
   `-Force`.
10. **Warn-only dependency check** in `bootstrap.ps1 [Claude]`: the moved
    conventions reference local, unsynced files. Bootstrap prints a
    `[WARN]` (never creates, copies or edits) for each of these missing
    from `~/.claude`: `CONFIG_RESOLUTION.md`, `KNOWLEDGE_BASE_BUILDER.md`,
    `commands/kb-init.md`, `commands/kb-build.md`, `commands/kb-query.md`;
    plus the duplicate-heading warning from Step 1.3. Warnings don't fail
    the bootstrap.

### 2. Project `dnachem-min`
1. `update-compile-cmd.ps1`: read `geant4InstallPath` from
   `.claude-project.json` (expand the variables from the env) instead of
   hardcoded `Geant4_DIR` / `CMAKE_PREFIX_PATH`. Validation stays in
   dotfiles; the ps1 only carries a bare "are the vars set?" fail-safe (no
   path checks): at the top, `if (-not $env:DEV_DIR -or -not $env:G4_ROOT)
   { throw "DEV_DIR / G4_ROOT not set — run ~/dotfiles/bootstrap.ps1 and
   restart your shell" }`. Without it, an unset var expands to an empty
   string and CMake fails on a nonsense path like `/lib/cmake/Geant4`.
   **Decided: keep.**
2. **G4Vox lines: keep**, only re-rooted per the path map (CMake untouched).
   They are lines 65–66 of
   `C:\DEV\GEANT4\SIM\dnachem-min\.claude\hooks\update-compile-cmd.ps1`
   (`-DG4Vox_DIR=…/LIB/G4Vox-install/lib/cmake/G4Vox` and the second entry
   of `CMAKE_PREFIX_PATH`). Current `dnachem-min` `CMakeLists.txt` does not
   use G4Vox, **but** these CMakeLists do (`find_package(G4Vox REQUIRED)`):
   `C:\DEV\GEANT4\SIM\VoxChem\`, `C:\DEV\GEANT4\SIM\GeoVox\`, and the
   `C:\DEV\GEANT4\SIM\dnachem-min.worktrees\{update-sim-cc-unamur-comment,
   auto-update-compile-commands-json, add-file-creation-date-comment,
   check-posttooluse-hook-functionality}\` worktrees — where this hook
   would fail to configure without the G4Vox path. Hence: keep.
3. `.claude/settings.json` hook → exec form with
   `${CLAUDE_PROJECT_DIR}/.claude/hooks/update-compile-cmd.ps1`.
4. `.claude-project.json`: every absolute path → its path-map expression.
5. Permissions (**decided by Step 0: 2b**): remove the absolute
   `additionalDirectories` from the tracked `settings.json` (env vars are
   not expanded there); they are generated per machine into the untracked
   `.claude/settings.local.json` by `init-claude-project.ps1`. The deny
   rule becomes the tracked, drive-independent
   `Edit(//**/geant4-v11.4.1-source/**)` (replacing
   `Edit(C:/DEV/GEANT4/geant4-v11.4.1-source/**)`).
6. Git hygiene: `git rm --cached` `.claude/hooks/hook-posttooluse.log` and
   `compile_commands.json`; `.gitignore` += `.claude/hooks/*.log`,
   `.claude/settings.local.json`, `.claude/scheduled_tasks.lock`.

### 3. Local-only edits (not synced; repeat by hand on each machine)
- `~/.claude/CLAUDE.md`: after Step 1.5 adds the import, **delete the
  "Global Geant4-DNA Conventions" body** from the local file (it now lives
  in `COMMON_CLAUDE.md`), keeping only the import line and anything
  machine-specific. Done by hand once per machine, with a backup; bootstrap
  only warns.
- ~~`~/.claude/commands/g4-new.md` hand edit~~ **dropped**: that file is
  *generated* per machine by `<template>\scripts\install-command.ps1`
  (substitutes `{{TEMPLATE_PATH}}`), so a hand edit would be overwritten and
  other machines simply re-run the installer. `setup-claude.ps1` instead
  **warns** when the recorded template path does not exist on the machine.
- `CONFIG_RESOLUTION.md`, `commands/`, `skills/`, `settings.json` otherwise
  untouched.

### 4. New-machine bootstrap
1. Git, Node, VS Build Tools, Claude Code; GitHub SSH key (dotfiles
   `ssh/config` + `.bashrc` already load keys).
2. `git clone git@github.com:rtonneau/dotfiles.git ~/dotfiles`;
   `~/dotfiles/bootstrap.ps1` (the **existing** sections still create
   symlinks → elevated shell or Developer Mode, as today; the new
   `[Claude]` section only needs junctions; answer the settings-sync
   prompt); restart shell.
3. `claude` → log in; install plugins/skills/commands on that machine as
   today (**not** synced by this plan).
4. Clone each project under `${DEV_DIR}/GEANT4/SIM/`; run
   `init-claude-project.ps1` in it.

## Verification
- `check-env.ps1` by hand: both OK; `DEV_DIR` unset; `G4_ROOT` unset;
  `G4_ROOT` → empty dir; `DEV_DIR` without vcpkg; `G4_ROOT` version with no
  matching source dir; var in User scope but not in the process. Distinct
  message + exit code each.
- `bootstrap.ps1` twice on a scratch copy of `~/.claude`: 1st run changes
  only `hooks` link, one top-of-file import line, added hook entries (diff
  shows nothing else); 2nd run is all `[SKIP]` / "in sync". Also: file
  without the import, file with it in the middle, file with it inside a
  code fence (must be treated as absent), CRLF file, missing file,
  `hooks/` real dir with foreign files (must stop).
- `sync-claude-settings.ps1` on scratch copies of `settings.json`:
  identical → "in sync", no prompt; one missing hook → listed, prompt
  shown; **answer `n` → file byte-identical** (hash); answer `y` → backup
  exists, parsed JSON equals the original **plus only the listed items**,
  key order preserved, 2nd run "in sync"; scalar conflict (`model`) →
  listed with both values, **per-item prompt**, `n` leaves it unchanged,
  `y` changes only that key; missing-items `y` + conflict `n` (and the
  reverse) work independently; `-Yes` alone never overwrites a conflict;
  hook already present under another group →
  not duplicated; array-of-scalars diff (`permissions.allow`); invalid
  local JSON → abort, file untouched; local file missing → offer create;
  no TTY without `-Yes` → no hang, no write; `-Check` → exit 1 when
  something is missing.
- Sentinel in `COMMON_CLAUDE.md` is answered in a new session; the
  Geant4-DNA conventions appear **exactly once** in the loaded context
  (no duplicate from the local `CLAUDE.md`); bootstrap warns when the
  local file still has the `# Global Geant4-DNA Conventions` heading.
- Vars unset → session-start warning; ps1 hook and `/kb-*` fail explicitly.
- `grep` confirms validation logic exists only in
  `dotfiles/claude/hooks/check-env.ps1`.
- `git diff` shows no `CMakeLists.txt` change in any repo.
- Edit a `.cc` → `compile_commands.json` regenerated (check ps1 output).
- Simulate another machine: junction `D:\DEVTEST` → `C:\DEV`,
  `DEV_DIR=D:\DEVTEST`, `G4_ROOT` = a *different* junction of the install
  dir; re-run `init-claude-project.ps1`; hook, `/kb-query`, directory
  access work with no other edit.
- `git status` clean in both repos after an edit session.

## Non-goals / risks
- **Not synced:** anything not in `common_settings.json`, plus commands,
  skills, KB docs, memory. Drift between machines is accepted. Sync is
  **add-only and on request**: removing or changing a setting in
  `common_settings.json` later does *not* remove/change it on machines
  that already applied it (by design, no deletes).
- `sync-claude-settings.ps1` needs **PowerShell 7 (`pwsh`)** on every
  machine (5.1's `ConvertTo-Json` escapes `&`, `<`, `'` and lacks
  reliable depth/format control); bootstrap must check and say so. The
  registered hook command itself keeps `powershell.exe` (5.1), matching
  the existing project hook.
- The first accepted sync re-serialises `settings.json` (formatting may
  change once; content and key order are preserved; a backup is kept).
- The `~/.claude/hooks` **junction**: docs say Edit/Write refuse to write
  through a *symlink*; whether that applies to junctions is untested. If
  it does, editing a hook from Claude means editing the dotfiles target.
- Import not loaded in Claude Desktop **Cowork** sessions (see facts).
- Variables in `.claude-project.json` are expanded by Claude following
  `COMMON_CLAUDE.md` (procedural, not enforced); the ps1 is the only
  code-enforced consumer.
- Geant4 version is embedded in literal path segments
  (`geant4-v11.4.1-source`, `-kb`) while `G4_ROOT` carries its own
  version; upgrades must update both (validator flags a mismatch).
- Per-project **memory** is keyed by absolute project path; not synced.
- **Git worktrees** (`dnachem-min.worktrees\*`) each check out their own
  `.claude/` at their own commit: they won't get the ps1/settings changes
  until rebased/merged, and each needs its own
  `init-claude-project.ps1` run (the generated `settings.local.json` is
  per worktree).
- Out of scope, noticed: presets could use `$env{DEV_DIR}` / `$env{G4_ROOT}`;
  `scripts/bootstrap-prompts.ps1` points at a non-existent
  `~/dotfiles/prompts`; `bootstrap.ps1` header says `install.ps1`.

## Execution log (2026-09-24)

### Built
Dotfiles repo `~/dotfiles`, branch `claude-config` (uncommitted; your 2
unrelated modified files are untouched):
- `claude/hooks/check-env.ps1` (validator; PS 5.1-compatible), `claude/hooks/plugins-version.sh`
  (byte-identical copy), `claude/COMMON_CLAUDE.md`, `claude/common_settings.json`
- `scripts/sync-claude-settings.ps1`, `scripts/setup-claude.ps1`,
  `scripts/setup-dev-env.ps1`, `scripts/init-claude-project.ps1`
- `bootstrap.ps1`: `-DevDir/-G4Root` params + `[Env]` and `[Claude]` sections
  (they call the scripts above; the logic lives in scripts, not inline)
- `.gitattributes`: `claude/hooks/*.sh text eol=lf` (your `core.autocrlf=true` would
  otherwise break the hook under Git Bash)

Project `dnachem-min` (working tree only, on `feat/macro-dump-and-reset`):
`update-compile-cmd.ps1` (env-var driven + fail-safe), `settings.json` (exec-form hook,
drive-independent deny, no absolute dirs), `.claude-project.json` (variables),
`.gitignore` (+3 entries), index: `hook-posttooluse.log` and `compile_commands.json`
untracked (files kept on disk); untracked `.claude/settings.local.json` generated.

Template repo `geant4-dna-template` (working tree, `main`): `scripts/new-project.ps1`
now expands `${DEV_DIR}` / `${G4_ROOT}` from `-FromConfig` (**not in the original plan**:
without it `/g4-new -FromConfig` would have broken on the new placeholders).
No `CMakeLists.txt` was touched anywhere.

Your real `~/.claude` (all with backups in `~/.claude/backups/`): `hooks` is now a junction
to `dotfiles\claude\hooks`; `CLAUDE.md` = the single import line (conventions moved verbatim
to `COMMON_CLAUDE.md`, checked identical first); `settings.json` gained the `check-env`
SessionStart hook (1 item; nothing else changed). `DEV_DIR=C:\DEV` set at User scope
(`G4_ROOT` already was).

### Deviations from the plan
- `g4-new.md` hand edit dropped (generated file — see Step 3).
- `sync-claude-settings.ps1` gates on **strict** `Test-Json` (PowerShell's parser accepts
  trailing commas/comments that Claude Code rejects, and would silently "repair" them),
  and takes `-Answers 'y,n'` (comma-separated; `pwsh -File` drops extra array values) for
  automation/tests.
- Junction, not symlink, for `hooks/` (symlinks need admin here).
- Step 1.4's "hooks registration" is done by the settings sync, as decided.

### Verified (all on scratch copies unless stated; real files only touched where noted)
- `check-env.ps1`: 10 cases in pwsh 7 and Windows PowerShell 5.1 (OK, unset, not-visible,
  empty dir, no vcpkg, version without source, odd leaf, `-Hook` silent/JSON).
- `sync-claude-settings.ps1`: **42/42** (incl. real settings copy; n = byte-identical;
  per-item conflict prompts; independence; arrays; 4 kinds of invalid JSON; BOM; case-
  differing keys; special characters; no-TTY never blocks).
- `setup-claude.ps1`: **40/40** (real-`~/.claude` copy byte-exact incl. CRLF; every import-
  detection edge case; hooks-dir safety; `-WhatIf` changes nothing; PS 5.1).
- `setup-dev-env.ps1` + `init-claude-project.ps1`: **20/20** (fake DEV tree in `%TEMP%`).
- Project hook: non-`.cc` skipped, fail-safe fires with vars unset, real CMake reconfigure
  gives **identical** `Geant4_DIR` / `G4Vox_DIR` / `CMAKE_PREFIX_PATH` cache values.
- `new-project.ps1 -NoBuild -NoGit`: same absolute values in the generated project as before;
  clear error with vars unset; no leftover `${...}`.
- Real headless sessions: `COMMON_CLAUDE.md` loads via the import (conventions heading appears
  **once**, plus the new Path-variables section); both `SessionStart` hooks run through the
  junction (`check-env` silent when healthy, warns when `DEV_DIR` is missing).
- Step 0(a)–(e) results: see "Step 0 results".

### NOT done / follow-ups
- **Committed locally, NOT pushed** (order matters: template before project):
  1. `geant4-dna-template` branch `fix/expand-path-variables-from-config` → `497ea9d`
  2. `dotfiles` branch `claude-config` → `efa1bc8` (your 2 unrelated modified files not included)
  3. `dnachem-min` branch `chore/portable-claude-config` → `40ccd5a`, built in the worktree
     `dnachem-min.worktrees\portable-claude-config` off `main` (the feature branch changes
     `.claude-project.json` in other sections, so committing from it would have mixed
     the two). The same edits are still uncommitted in the `feat/macro-dump-and-reset`
     working tree (harmless duplicates, deletions unstaged); before merging `main` into
     that branch, `git restore` those 4 files (keep `hook-posttooluse.log` /
     `compile_commands.json` content) or stash them.
  Pushing / merging to `main` (needed for other machines to clone the dotfiles) is left to you.
- **Not tested on a second physical machine.** Simulated with a fake DEV tree in `%TEMP%`
  and env overrides; the "junction `D:\DEVTEST`" scenario was not run.
- Projects **generated** by `/g4-new` still get absolute paths in their own
  `.claude/.claude-project.json` / `settings.json` (as before). Making the template emit
  `${DEV_DIR}`/`${G4_ROOT}` (and a template-side `init-claude-project` step) is a follow-up.
- Other checkouts need their own run of `init-claude-project.ps1` (each git worktree in
  `dnachem-min.worktrees\*`, plus `VoxChem`, `GeoVox`, which have their own hardcoded G4Vox paths
  and were not touched).
- Claude Desktop **Cowork** sessions skip the user-scope import, so the conventions won't
  load there (docs).
- Test harnesses live in `%TEMP%\sync-tests\` (not committed); they can be moved into
  `dotfiles/tests/` if wanted.

## Open decisions
None. All decisions are resolved.

Resolved: unsynced files referenced by the conventions
(`CONFIG_RESOLUTION.md`, `KNOWLEDGE_BASE_BUILDER.md`, `/kb-*` commands) =
**warn only** (Step 1.10); project ps1 "are the vars set?" fail-safe =
**keep**; hook registration = compare/show/ask via
`sync-claude-settings.ps1`; `common_settings.json` = hooks only for now;
conflicts = offered item by item; import at the **top**;
`COMMON_CLAUDE.md` includes the existing Geant4-DNA conventions; G4Vox
lines kept and re-rooted (used by VoxChem, GeoVox and 4 worktrees).
