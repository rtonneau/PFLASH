# Explorer: bring it up to date with the Output products (ADR 0009)

On approval this file is copied to `.claude/plans/2026-11-05-explorer-output-update.md` (project rule).

## Context

The output code moved to seven **Output products** with selectable formats (ADR 0009), but `docs/explorer` only partly followed:

- **Module map:** the output nodes added for this work (`product-output`, `output-formats`, `output-table`, `hdf5-table-file`, `root-table-file`, `meso-spatial-root-rows`, `prechemrecorder`) have no connections at all, so the map does not show how a dump reaches a file.
- **Run flow and node text:** still names `.Txt` files, says `output-table` is unused ("Nothing uses it yet"), and calls `meso-spatial` HDF5-only.
- **Macro builder:** output settings are mixed into the "Runs and output" step as eight long lists of checkboxes. Turning a product off means ticking a `none` box, and nothing shows which files you will get.

Goal: the map shows the real output architecture, the text matches the code, and the builder has its own Output step for choosing which products to write and in which formats.

No change to the simulation code or to the macro commands.

## 1. Macro builder: a new `output` step

**Steps** (`docs/explorer/macro-builder.js`, `STEPS`): add `output` between `runs` and `review`. `runs` becomes "Runs" and keeps sub-runs, seeds and dumps. Existing `(step 2)` text stays right because earlier steps do not move.

**Fields moved to `step: 'output'`:** `outputDirMode`, `outputDir`, `defaultFormats`, all `fmt_<product>`. Validation issues take their step from the field, so they follow automatically. Add `output` to `STEP_ICONS` in `template.html`.

**Sketch of the step:**

```text
Output folder       (•) Default  ( ) Fixed in the macro
Default formats     [ ] txt  [x] csv  [ ] h5  [ ] root  [ ] none

Product              Write                  txt csv h5 root   You get
Species              [Default ▾]             -   x   -   -    Species.csv
SpeciesMeso          [Own formats ▾]         -   x   x   -    SpeciesMeso.csv, .h5
Reactions            [No file ▾]             -   -   -   -    no file
...
PreChemical          [Default ▾]             -   x   -   -    PreChemical_run<R>_event<E>.csv
SpeciesMesoSpatial   [No file ▾]                     -   -    no file (h5, root only)
```

**Row behaviour.** Each row has a selector with Default formats / Own formats / No file. `SpeciesMesoSpatial` has only Own formats / No file, because it never follows the default.

**No new saved state.** The selector is derived from the existing `formats.<id>` list, so generated commands and older saved settings are unchanged:

| Stored list | Selector | Command written |
|---|---|---|
| `[]` | Default formats (spatial: No file) | none |
| `['none']` | No file | `/output/<id>/format none` |
| `['csv', 'h5']` | Own formats | `/output/<id>/format csv h5` |

- Choosing Own formats starts from the formats the product currently gets, or `csv` if that is empty (spatial: `h5`).
- Unticking the last format gives No file. It never silently falls back to the default.

**New model functions in `macro-builder.js`** (the template stays free of rules):

- `productMode(cfg, id)`, `setProductMode(cfg, id, mode)`, plus the last-format rule.
- `effectiveFormats(cfg, id)`, mirroring `OutputFormats::Effective` (own list, else default; spatial never follows the default; a product skips formats it does not accept).
- `outputFiles(cfg, id)` for the "You get" column:
  - `Reactions` txt/csv also lists `ReactionsCatalog`.
  - `PreChemical` txt/csv is per event; its h5/root is one file per dump.
  - Meso products under SBS or meso off show "not written", with the existing reason.

**New validation:** a warning when no product writes anything ("only `Manifest.json` is written"). Existing format and h5 checks are kept. `summary()`'s "Output formats" row reports the effective choice, including products with no file.

**Rendering (`template.html`):** a new `type: 'products'` field rendered as a table by `renderOutputProducts`, replacing the seven per-product fieldsets. It keeps the keyboard focus, issue links and technical-details pattern. The `fmt_<id>` field ids stay, so issue links and preview highlighting still work. On narrow screens the table reflows into stacked rows.

## 2. Module map links (`architecture.json`, `edges`)

Edges mean "from uses/creates/fills to". Added only where a call site confirms it, not from includes alone. Confirmed so far:

| From | To | Evidence |
|---|---|---|
| `dump-msg` | `product-output` | `ProductOutput::Write`, `RunAccumulatorMessenger.cc:44` |
| `dump-msg` | `output-formats` | `Effective`, `:64`, `:97` |
| `dump-msg` | `score-species` | `ToTable`, `:44` |
| `dump-msg` | `prechemrecorder`, `meso-spatial-root-rows` | `TakeStaged`, `:65`, `:102` |
| `dump-msg` | `hdf5-table-file`, `root-table-file` | direct PreChemical and spatial writes, `:81-82`, `:108` |
| `dump-msg` | `meso-spatial` | `MoveStaged`, `:150` |
| `product-output` | `output-formats`, `output-table`, `hdf5-table-file`, `root-table-file` | `ProductOutput.cc:63-93` |
| `product-output` | `outputdir` | named in its summary; check the call site |
| `hdf5-table-file`, `root-table-file` | `output-table` | `Write` signature takes `OutputTable` |
| `score-species`, `meso-counter`, `reaction-counter`, `phys-counter`, `track-length` | `output-table` | `ToTable(s)` in their headers |
| `prechemrecorder`, `meso-spatial-root-rows` | `output-table` | `MakeTable` |
| `prechemrecorder` | `output-formats`, `prechem` | `PreChemicalRecorder.cc:117-131` |
| `meso-spatial-root-rows` | `meso-spatial` | `MesoSpatialRootRows.cc:35` |
| `event-action` | `prechemrecorder`, `output-formats`, `prechem` | `EventAction.cc:112-156` |
| `time-step` | `meso-spatial-root-rows`, `output-formats` | `TimeStepAction.cc:142`, `:660-676` |
| `manifest` | `output-formats`, `meso-spatial` | `RunManifest.cc:127-157`, `:273` |
| `output-formats` | `meso-spatial` | `OutputFormatsMessenger.cc:32` |
| `sim` | `output-formats` | `sim.cc:142` |

To check at the call site before adding: `accumulator` to its counters and `track-length`, `manifest` to `accumulator` and `outputdir`, `score-species` to `outputdir`. Any edge already present is left alone.

**Out of scope:** missing links unrelated to output, such as `chem-list` to its messengers. I'll list them in my final report rather than widen this change.

**Regression guard:** `check.js` (the `ExplorerCheck` ctest) fails when a node in the scoring lane has no connection, since that is exactly what went wrong here. If unrelated nodes in that lane already have none, I'll report them rather than add links outside this scope.

## 3. Stale text (`architecture.json`)

**Flow steps:**

- `physics`, `event-end`, `meso`: `.Txt/.csv (recorded)` becomes `<Product>.<ext> (recorded)`.
- `prechem`: outputs cover txt/csv per event and h5/root per dump. Add `prechemrecorder` to `parts` and `PreChemicalRecorder.cc` to files. Fix "writes its line to the per-event file".
- `meso`: add the root variant and `meso-spatial-root-rows`. "Mean counts" becomes per-event statistics.
- `dump`: add `product-output`, `output-formats` and the table writers to `parts`, and `ProductOutput.cc` to files. Add the `/output/<product>/format` command.
- Commands `/output/*` and `/run/outputDir` get `builder: "output"`.

**Node summaries (`role` stays within 40 characters):**

- `output-table`: remove "Nothing uses it yet", say who uses it.
- `meso-counter`: per-event counts and G values with statistics.
- `meso-spatial`: role and summary describe the h5 path, with root going through the root-rows node.
- `dump-msg`: mention `ToDir` and the per-product writing.
- `prechem`: mention the h5/root tables moved with the per-event files.
- `manifest`: results index and `outputFormats`.
- `score-species`, `phys-counter`: mention the table output.

**Tasks and test lane:**

- `hdf5` task: list the table writers, not only `MesoSpatialFile.cc`.
- `read` task: link `docs/output/tables-h5-root.md`.
- Add a task "Choose output files and formats" pointing to `#builder/output`, in the shape existing tasks use. Drop it if tasks cannot link to the builder.
- Test lane: add the output test files missing from `t-score`/`t-core` (`OutputFormatsTest`, `OutputTableTest`, `ProductOutputTest` and others), matching how that lane lists tests today.

## 4. Docs

- `docs/explorer/README.md`: the Macro builder paragraph and rule lines gain the Output step.
- Check the other `.md` files that mention the Macro builder (`README.md`, `CLAUDE.md`, `docs/release.md`, ADR 0009, `CHANGELOG.md`) and change only what names the old step. Past plans and `.work/` stay as they are.

## 5. Tests (written first, `macro-builder.test.js`)

- `STEPS` order has `output` right before `review`; format and folder fields have `step === 'output'`; every product has a row.
- `productMode` round-trips through all three modes, and the last-format rule gives `['none']` (spatial: `[]`).
- `effectiveFormats` matches `OutputFormats::Effective` cases: default, own, `none`, default `none`, spatial ignoring the default, spatial skipping txt/csv.
- `outputFiles`: Reactions catalogue, PreChemical per event against per dump, meso off.
- The generated macro is unchanged for the default config and for the existing format tests.
- An old saved config normalises to the same commands.
- The all-products-off warning.

## Verification

1. `node docs/explorer/build.js`, then `check.js`, `macro-builder.test.js`, `catalog-view.test.js`, `export-catalog.test.js`.
2. From `build-ninja/`: ctest `ExplorerCheck` and `MacroBuilderTest` (project procedure, `.claude/geant4-instructions.md`).
3. Cross-check against the real parser: run a builder-generated macro with mixed modes through `sim.exe` (10 keV e-, `/run/beamOn 2`, `/scheduler/endTime 1 ms`) and compare the files written and `outputFormats` in `Manifest.json` with the "You get" column.
4. Browser check of `index.html` with the preview tools (approving this plan counts as the request):
   - the new step, keyboard use and a narrow window;
   - selecting `dump-msg` and `product-output` in the Module map;
   - links from issues in Review back to the Output step.

## Delivery

Work on a branch (`docs/explorer-output-step`), since `main` is the current branch. No commit, push or PR unless you ask.
