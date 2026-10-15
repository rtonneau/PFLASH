# g4run (simulation runner skill)

`/g4run` is a global Claude skill (not part of this repo) that queues, launches,
tracks and analyses Geant4 simulation runs through one SQLite database. The tool
is one Rust binary in its own future repo (`g4run`); analysis scripts stay Python.
The standalone specification (pinned to dnachem-min branch `Meso`) lives in the
dotfiles repo until it is copied into that repo:

- Spec: `~/dotfiles/claude/skills/g4run/SPEC.md` (branch `docs/g4run-spec`)
- Status: specified, not built yet.

Spec commits in dotfiles:

- ticket 01, core (scope, config, storage, schema, lifecycle, enqueue, selectors): `5b45bfc`
- ticket 02, execution (dispatcher, admission, run wrapper, failures & requeue, cancel, prune, notifications): `2cd37f9`
- ticket 03, analysis contract, subcommand reference, installation, build milestones: `bc73b8a`
- revision, Rust implementation + standalone spec: `019356e`
