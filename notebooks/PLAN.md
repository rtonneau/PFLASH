# dnachem-min demonstration notebooks: implementation plan

## Goal

Create reproducible, explanatory Jupyter notebooks that demonstrate mining and
visualizing dnachem-min output across the physical, physico-chemical and
chemical stages. Use G4Utils for reusable data access and analysis; improve it
where the notebook work establishes a need. The first pass is a vertical slice
and a report, followed by library work and the final notebook series.

## 1. Establish the demonstration data and environment

- [x] Inspect the current output documentation, G4Utils interfaces and Jupyter
  kernel before relying on their behavior. Use Jupyter MCP for notebook
  creation, editing and execution.
- [x] Select or generate a small, reproducible run with a documented macro,
  seed, chemistry mode and output directory. It must provide physical,
  pre-chemical and chemical data. Record the simulation and Geant4 versions,
  event count and commands needed to reproduce it. Treat small-run plots as
  workflow demonstrations, not converged scientific results.
- [x] Identify an additional mesoscopic run and, for the optional spatial
  notebook, a run with spatial output enabled. Avoid committing large generated
  datasets; document how to obtain them and make their paths configurable.
- [x] Record the Python environment, G4Utils installation and notebook
  dependencies. Verify imports in the actual Jupyter kernel.

**Done when:** the chosen data and kernel are accessible, prerequisites and
reproduction instructions are documented, and each required output is checked
on disk rather than inferred from its manifest alone.

## 2. Build and execute the first notebook pass

- [x] Create one compact vertical-slice notebook with a user-editable dump
  path. Display provenance, settings, events, deposited energy and available
  files before analyzing results.
- [x] Demonstrate physical interaction counts and event-level track lengths;
  pre-chemical water modifications and solvated-electron positions; and
  species evolution and reaction counts. Include mesoscopic aggregate counts
  when available, identifying the hand-over and the observable being plotted.
- [x] Use existing G4Utils interfaces where they fit. Keep exploratory readers
  and calculations small and visible; document their assumptions and failures.
  Handle missing optional files with a clear explanation.
- [x] Compare representative parsed rows and calculations with the source
  files and the current simulation scoring code. Show units, normalization,
  sample size and applicable stage limitations beside the results.

**Done when:** the notebook runs top to bottom in a fresh kernel on the
documented dataset, produces checked plots for all three phases, and exposes
the remaining library gaps without presenting provisional results as validated
physics conclusions.

## 3. Write the G4Utils work report

- [x] Add the separate [G4UTILS_REPORT.md](G4UTILS_REPORT.md) in this directory, based on the executed first
  pass. For each gap, record the analysis it enables, actual input and current
  behavior, proposed interface and return data, units and normalization,
  optional/missing-file behavior, evidence, and acceptance checks.
- [x] Assess readers for physical interactions, track lengths, pre-chemical
  event records and aggregate mesoscopic species counts; assess dump discovery
  for results indexes, subdirectories and filename prefixes. Confirm each gap
  against the current implementation before proposing a change.
- [x] Investigate the distinction between the ratio of total counts to total
  energy and the mean of event-level G-values, including available information
  for uncertainty. Specify only estimators justified by the stored data.
- [x] Separate library responsibilities from notebook-specific visualization;
  prioritize additions and identify any unresolved scientific or format issues.

**Done when:** every proposed library change is tied to an observed notebook
need and has a testable interface and acceptance criteria; unresolved issues
are identified explicitly rather than filled with assumptions.

## 4. Improve G4Utils

- [x] Implement the report's justified reusable readers and calculations in
  the G4Utils repository, following its local instructions and conventions.
- [x] Add focused tests using representative formats and edge cases, including
  units, prefixes, missing optional files, event/run identity and normalization
  where applicable. Document public interfaces and installation requirements.
- [x] Run the relevant G4Utils tests and validate representative outputs against
  the original simulation files. Update the report with decisions, completed
  changes and any deferred limitations.

**Done when:** the selected library capabilities are documented, tested and
usable from the Jupyter kernel, with any deferred items recorded.

**Completed 2026-11-03:** 393 G4Utils tests passed in the editable GEANT4
environment used by the notebook, and all public imports passed there.
The opt-in validator matched both demo dumps against raw physical, track,
pre-chemical, particle and mesoscopic files, including the IRT 2,152/999 counts
and mesoscopic 97/2 normalization. Actual shared-folder prefixes, nested and
indexed historical output, collision manifests and explicit selection of an
omitted historical sibling were checked. The report's delivery update records
commands, evidence and remaining scientific/source-version limits. Existing
kernels need restarting to load the changed library; notebook replacement and
fresh execution remain in step 5. Use G4Utils branch
`feat/g4utils-report-improvements`, or a merged revision containing it, before
restarting; an editable checkout switched back to unmerged main lacks the APIs.

## 5. Complete the notebook series

- [ ] Replace exploratory parsing and reusable calculations with the validated
  G4Utils interfaces. Organize the material into:
  - `00` — simulation discovery and provenance;
  - `01` — physical interactions and track lengths;
  - `02` — physico-chemical records and spatial positions;
  - `03` — chemical species, yields and reactions;
  - `04` — optional mesoscopic spatial snapshots and export.
- [ ] Document inputs, setup, reproduction commands, navigation and expected
  output in a notebooks README. Keep paths configurable and optional analyses
  clearly marked.
- [ ] Execute each notebook from a fresh kernel against the documented data;
  inspect tables and plots, verify labels and units, and record the outcome and
  limitations. Check that missing optional data gives actionable guidance.

**Done when:** another user can follow the README to run the series, each
notebook has been executed and reviewed, and scientific limitations are
visible at the point of interpretation.

## Interpretation checks

- Physical process counts are aggregate discrete G4DNA interaction counts,
  not complete tracks or spatial interaction histories. Track-length rows
  describe non-aborted events.
- Pre-chemical files record water modifications and solvated-electron entries;
  verify their precise fields against the installed Geant4 source and do not
  treat them as a complete physical-step record.
- Particle-stage reaction counts omit the mesoscopic stage and the documented
  scavenger/acid-base reactions. Consult the current output documentation for
  the exact coverage.
- Mesoscopic aggregate counts and particle-stage G-values are different
  observables. Check event normalization, deposited-energy normalization and
  hand-over times before comparing or combining curves.
- For comparisons, inspect beam, chemistry, geometry, event counts and seeds;
  do not treat potentially repeated events as independent samples.

Current references: project `CLAUDE.md`, `.claude/skills/sim-output/SKILL.md`,
`docs/output/README.md`, `docs/output/SpeciesMesoSpatial-h5.md`, and the
G4Utils README and source. Recheck them during implementation as formats and
interfaces can change.

