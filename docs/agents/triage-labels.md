# Triage Labels

Minimal status vocabulary for this repo's solo local-markdown workflow. Used as
the `Status:` line value in issue files (see `docs/agents/issue-tracker.md`).

| Status               | Meaning                                                            |
| --------------------- | ------------------------------------------------------------------- |
| `ready-for-agent`    | Fully specified — an agent can implement it                       |
| `ready-for-human`    | Needs my decision or manual work (e.g. physics choices, golden-file approval) |
| `done`               | Implemented and its acceptance check passes                       |

The `triage` skill isn't used on this project; this vocabulary exists only to
give `Status:` lines a fixed, small set of values.
