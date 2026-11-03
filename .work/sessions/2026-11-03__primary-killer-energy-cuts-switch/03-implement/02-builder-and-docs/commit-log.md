# Ticket 02: builder-and-docs

**Status:** ✅ Done

## Local Test Result

`node docs/explorer/build.js` (wrote index.html); `node docs/explorer/macro-builder.test.js`: 24 passed; `ctest --test-dir build-ninja -R "ExplorerCheck|MacroBuilder"`: 2/2 passed.

## Review Notes

Off now writes `/primaryKiller/energyCuts false`; KILLER_OFF removed (constant, comment, export). Tech text, README, architecture.json (summary + command), CLAUDE.md updated; also updated `docs/recipes/dnachem-min-macro-command-reference.md` (not in the original file list: it described minKineticE as unhandled).

## Blockers / Challenges

None

## Commits

- fccd3ee feat(explorer): builder Off writes /primaryKiller/energyCuts false; docs (ticket 02)

## Time Spent

0m (ticket-start.js to ticket-complete.js)
