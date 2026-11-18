# Study tooling replaces g4run; study data outside git under STUDIES_ROOT

_ADR 0012, recorded 2026-11-18 (session 2026-11-18__formalize-simulation-studies)._

<!-- Format adapted from mattpocock-skills' domain-modeling (ADR-FORMAT.md): https://github.com/mattpocock/skills.git -->

Study provenance was hand-written prose, data sat in `.scratch/tests/<session>/`, and the g4run spec (queue, dispatcher, SQLite) stayed unbuilt and recorded neither the Geant4 install nor its patches. A light global `/study` skill with a one-run wrapper that writes a **Provenance stamp** (git commit and diff, exe snapshot and hashes, Geant4 install with its `g4-build-info.json` patch set, hardware, outcome) replaces g4run, whose spec is marked superseded; parallelism stays in project runners. All study data, analysis outputs and bulky figures live under the machine-local `STUDIES_ROOT`, never in git; the repo keeps only the **Study index** (paths as `${STUDIES_ROOT}/...`) and the report text with its few final figures.
