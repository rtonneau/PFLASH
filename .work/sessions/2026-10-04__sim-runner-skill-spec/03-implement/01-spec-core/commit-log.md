# Ticket 01: spec-core

**Status:** ✅ Done

## Local Test Result

`grep -E '^## ' SPEC.md; grep -c TBD SPEC.md` -> Purpose & scope, Configuration, Storage layout, Database schema, Run lifecycle, Enqueue, Selectors; TBD 0. PASS.

## Review Notes

Drafted by an opus subagent, reviewed here against all five criteria. Changes after review: project resolution no longer depends on the absent `~/.claude/CONFIG_RESOLUTION.md` (own rule stated, file wins if it appears); noted that `analysis/g4run_compare_reference.py` is an adapter still to write. Subagent-added defaults kept: `--yes` above 20 runs, `--dry-run`, `queued -> failed` on launch error, `~/.claude/g4run.json` global settings. Committed in dotfiles on branch `docs/g4run-spec` (SPEC.md only); the dnachem-min commit holds the session record only, since the file is outside this repo.

## Blockers / Challenges

`~/.claude/CONFIG_RESOLUTION.md`, referenced by COMMON_CLAUDE.md, does not exist on this machine.

## Commits

- 194f27c docs: g4run spec core in dotfiles, pointer doc (ticket 01)

## Time Spent

55m (ticket-start.js to ticket-complete.js)
