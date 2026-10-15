# Implementation Plan

**Session:** sim-runner-skill-spec
**Date:** 2026-10-04T08:48:33.120Z
**Estimated effort:** 2-3 hours

## Strategy

Write the `/g4run` specification as one markdown file in the dotfiles repo, section group by section group, then commit it there and point to it from dnachem-min. Spec only: no Python, DB, venv, dispatcher, SKILL.md or bootstrap symlink is built in this session. Data model first (everything else refers to its tables and states), then execution behaviour, then the analysis/CLI surface and build milestones, then the commit and pointer.

## Tickets Overview

- **Ticket 1:** SPEC.md core: scope, configuration, storage layout, DB schema, run lifecycle, enqueue.
- **Ticket 2:** SPEC.md execution: dispatcher, admission, run wrapper, failures/requeue, cancel, prune, notifications.
- **Ticket 3:** SPEC.md analysis contract, subcommand reference, installation, build milestones.
- **Ticket 4:** Commit SPEC.md in dotfiles (alone) and add a pointer in dnachem-min CLAUDE.md.

## Sequencing Rationale

The schema and states (ticket 1) are referenced by the dispatcher and wrapper (ticket 2); the subcommand reference (ticket 3) names flags and states defined in 1 and 2. The commit/pointer (ticket 4) needs the finished file.

## Risks & Mitigation

- **Risk:** dotfiles has uncommitted user changes (README.md, bootstrap.ps1, COMMON_CLAUDE.md, ...) that could be swept into a commit → **Mitigation:** stage only `claude/skills/g4run/SPEC.md` by explicit path; verify with `git show --stat HEAD`.
- **Risk:** spec leaves design choices implicit → **Mitigation:** each ticket's check greps for "TBD"; every open question of the grill is closed with a stated default in the spec.
- **Risk:** gps per-ticket commits happen in dnachem-min while the spec lives in dotfiles → **Mitigation:** tickets 1-3 also commit SPEC.md in dotfiles on branch `docs/g4run-spec`; the dnachem-min commit holds the session record.

## Assumptions

- dotfiles is a git repo at `~/dotfiles`; skills are not yet linked from it (`claude/skills/` does not exist); bootstrap.ps1 creates symlinks into `~/.claude`.
- The approved grill resume (`01-grill/resume.md`) is the source of every decision; the spec transcribes and details it, it does not reopen decisions.
