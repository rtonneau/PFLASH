# Architecture explorer

A clickable guide to the code for newcomers and non-coders: module map, run flow, task guides and a macro builder.

## Open it

Double-click `docs/explorer/index.html` (or open it in any browser). It works offline and needs no server.

Online: https://rtonneau.github.io/dnachem-min/explorer/ (GitHub Pages, branch `main`, folder `/docs`; `docs/.nojekyll` turns off Jekyll). Opened from disk, file links go to your working copy (`../../<path>`); served over http(s), including a local test server, they open the file on GitHub at `main`.

## Edit it

`architecture.json` is the only source of content; `index.html` is generated from it and `template.html`.

- `lanes[]`: one per source folder, with `nodes[]` (title, a short plain-language `role` shown under the title in the box, at most 40 characters, the fuller `summary`, `files`, `symbols`) and `otherFiles[]` for files that are not worth a box.
- `edges[]`: `{from, to}` node ids; `from` uses, creates or fills `to`. The map draws only the selected box's edges (teal solid arrows: what it uses; purple dashed arrows: what uses it). Arrowheads always point from the part doing the using to the part it uses. Blue marks selection; amber flags mark summaries awaiting review. These meanings are shared by the map, run flow and task details in the existing light theme.
- `phases[]`: `{id, title, summary, window}`, the groups of the run flow, in order: `setup`, `physical`, `prechem`, `hetero`, `meso`, `output`. `window` is the time window text (copied from `physics-catalog.json` `timeWindows` for the four stages).
- `flow[]`: the run steps, titled `"N. Title"` in order, each with a short `overview` for scanning, the full `summary` for details, a `phase`, an optional `condition` (when the step only happens in some modes), output file names (each ending `(staged)`, `(recorded)` or `(written at dump)`) and linked docs. A step may also have `catalog` (`physics`, `prechem` or `reactions`: which catalogue it shows), `commands[]` `{cmd, builder?}` (macro commands from the messengers in `src/`; `builder` is a Macro builder `STEPS` id and may be omitted) and `parts[]` (map node ids). Keep scientific qualifications in the summary, condition and mode notes; verify changes against source.
- Chemistry modes: one step (the chemistry mode, in the first phase because it is chosen before `/run/initialize`) has `branches[]` `{id, label, summary, default?}`; the run flow shows a chooser under it. A later step may have `modes` `{branchId: note}`: the note is shown on its card for that mode, and a mode missing from `modes` marks the step as skipped. Steps without `modes` run the same way in every mode.
- `tasks[]`: entry points that link the guides in `docs/recipes/` and the ADRs.
- `startHere`: `{map, flow, tasks}`, each a list of `{id, why}` shown above the view as suggested first clicks, with the plain-language reason first.
- `"review": true` marks a summary that still awaits a physics review (shown with a dashed outline and a ⚑ marker).

The page derives the rest from the file lists: a box's run steps and tasks are those that share one of its files, so keep `files` accurate. Selecting a part opens **Connections only** by default: incoming parts sit to its left and outgoing parts to its right, with the source folder shown on every card. Ordered curves stay in the empty gaps between columns. Hover or keyboard-focus a connected part to emphasize its arrow. On narrow screens, labeled groups list the same relationships without long routes. Starting-point suggestions are hidden while focused to leave room for the connections.

Toggle **Connections only** off to return to the full folder map; that choice persists as you select other parts. The full map keeps rounded routes in reserved folder gutters and routes connections across non-adjacent columns above the folders. Starting a search returns to the full searchable map; Back restores the connection view and keyboard focus.

Relationship group headings name the selected part so direction stays clear when its card scrolls out of view. When the explanation sits below the connections, **Read explanation and files** jumps to its return navigation. **Back to connections**, or Escape while focused in that explanation, returns to the selected part without clearing it. Opening a different map selection through its URL brings that part into view.

The map's Find box searches titles, roles, summaries, file names and symbols. Matching parts stay grouped by folder; Clear search or Escape restores the full map. Following a connection to a filtered-out part clears the search so the destination is visible; Back restores the previous search. Links such as `index.html#map/time-step` or `#flow/dump` open a view with an item selected.

The detail panel's **Back to …** action follows browser history. During the current page session it restores the search, scroll position and keyboard focus. Tabs remember selections and retain the map search. Each view has a visible introduction and suggested starting points on desktop and narrow screens. On narrow screens, selection opens a dedicated detail view; **Back to Module map / Run flow / Tasks** (or Escape) returns to the selected item. Arrow keys switch tabs; Enter opens the first search match. The keyboard skip link goes straight to the explorer. Copy-path buttons announce success or failure to screen readers. The chosen chemistry mode is kept while the page is open. Full run explanations, per-mode comparisons and output names live in details.

## Run flow catalogue

The Run flow is grouped in six phases (setup, physical, physico-chemical, heterogeneous particle-based, mesoscopic, output), each with a time window line. A step with a `catalog` field shows a table in its details: `physics` (processes, models and energy ranges of `G4EmDNAPhysics_option2` for the chosen beam particle), `prechem` (dissociation channels, electron solvation, product displacement, initial species) or `reactions` (the full reaction table of the chosen Chemistry). The step's `commands[]` are listed with a link to the Macro builder step when `builder` is set, and its `parts[]` link to Module map boxes.

Two choosers sit above the tables and are kept for the page session with the chemistry mode: **Chemistry** (any registered Chemistry) and, for the physics table, **Beam particle** (`e-` or `proton`). The same reaction tables are reachable from a Chemistry's box in the Module map and from the Chemistry options of the Macro builder.

### Catalogue files

| File | Source | Edit by |
|---|---|---|
| `catalog.json` | the live chemistry, exported by `sim` (`/chem/reaction/exportJson`, `src/chemistry/CatalogExport.cc`) for every Chemistry of `macro-builder.js` | regenerate, never by hand |
| `physics-catalog.json` | hand-written from the Geant4 11.4.1 source (processes, models, ranges, solvation, displacement, time windows); each row has a `ref` | hand, verify against source |
| `catalog-view.js` | view model: turns the two files into table rows, rate-constant text and filters (`catalog-view.test.js`, `CatalogViewTest` ctest) | code |

Regenerate `catalog.json` after any change to a Chemistry's molecules, reactions or rates (`src/chemistry/catalog/*.cc`, `DnaChemistryList.cc`), then rebuild the page:

```bash
node docs/explorer/export-catalog.js --sim build/sim.exe    # rewrite catalog.json
node docs/explorer/export-catalog.js --sim build/sim.exe --check   # compare only
node docs/explorer/build.js && node docs/explorer/check.js
```

The generator runs `sim` once per Chemistry with a four-line macro (`/chem/select`, `/process/chem/TimeStepModel IRT_syn`, `/chem/reaction/exportJson`, `/run/initialize`), 300 s limit each, and refuses an export whose `timeStepModel` is not `IRT_syn` (the type column is the IRT_syn type). If `sim` cannot start, is killed or times out, the message says so, followed by the tail of its output. Before running anything it fails when the `sim` path does not exist, or when any file under `src/chemistry/` or `header/chemistry/` is newer than the `sim` binary (rebuild `build/` first).

Rate constants: every reaction and bulk reaction has an `order`. Order 1 is a reaction with an H2O partner, which Geant4 uses as a first-order rate (`G4DNAScavengerProcess` takes the rate itself as the propensity; the mesoscopic Gillespie counts Avogadro x volume water molecules per cell), so its constant is `k_s-1` in s^-1. Order 2 is every other pair, with `k_M-1s-1` in M^-1 s^-1. Each row has exactly one of the two keys, and the tables print the unit per row.

Notes on what the tables show:

- Bimolecular tables exclude `mesoOnly` reactions: bulk pairs that exist only in the mesoscopic reaction table. They are listed under **Bulk reactions**.
- A bulk reaction whose partner is also a tracked species (bulk O2 is the tracked `O2`) has the same pair as a bimolecular reaction; the driver shares that entry, so the mesoscopic table holds the pair once. Such bulk rows show **Also bimolecular: Yes** (`sharesTracked` in `catalog-view.js`).
- Two type columns (IRT_syn, SBS). The SBS column is derived, always 0 (fully diffusion-controlled), and is not exported; the IRT_syn column is the exported type.
- Rows of `physics-catalog.json` marked `"review": true` show an "unverified" badge.

### CatalogExportCheck

The `CatalogExportCheck` ctest runs `export-catalog.js --check`: it regenerates the catalogue from a live `sim` and fails when the committed `catalog.json` differs (rate constants compared to a relative tolerance of 1e-9, everything else exactly). When the ctest tree is Debug (`build-ninja/`), it runs the RelWithDebInfo `build/sim.exe` instead of the Debug target: the Debug sim crashes at `/run/initialize` (its CRT is mixed with the release Geant4 install). Override the executable with `-DCATALOG_EXPORT_SIM=<path>` at configure time. So `build/` must be built and current. A missing `build/sim.exe` fails with `sim not found`. A `build/sim.exe` older than any file under `src/chemistry/` or `header/chemistry/` fails with the list of newer files and a request to rebuild `build/`: the comparison alone would pass for a stale binary whose output still matches the old `catalog.json`. The age test uses file modification times, so it cannot see a change outside those two directories. `CatalogExportScriptTest` covers the comparison, the age test and the IRT_syn guard without a sim.

## Macro builder

The **Macro builder** tab (`index.html#builder`, or `#builder/<step>`) guides the reader through writing a simulation macro (`.in` file) for the current code: basics and logging, Chemistry model, timing and mesoscopic stage (two separate groups), water environment, beam, runs and output, then review and export. It is a view of its own (the Module map is not shown under it); the guide sits beside a live preview of the macro, and on narrow screens a Guide / Preview switch shows one at a time. Each Chemistry option links to its full description in the Module map.

Everything the builder knows lives in `macro-builder.js`: the steps, every setting (label, plain help, technical note, source references, default, units), the availability rules, the validation and the macro generation. The page renders the guide, the error markers and the preview from that one model, and `build.js` inlines it into `index.html`, so the page stays self-contained. When a messenger, default or rule changes in the C++ code, update `macro-builder.js` and its tests (`macro-builder.test.js`, run by the `MacroBuilderTest` ctest), then rebuild.

Rules it encodes, all verified against the source (each setting's **Technical details and sources** lists the files):

- PreInit commands come before `/run/initialize`; `/scheduler/endTime`, the gun and `/primaryKiller/*` (created at initialization) come after it; each sub-run is optional seeds, `/run/beamOn`, then an optional dump.
- The time-step model decides the mesoscopic stage: choosing SBS turns the stage off automatically (its control shows Off with the reason, and nothing is written for it, since an explicit `/chem/meso/enable true` under SBS is fatal); the IRT_syn choice is kept for a switch back. Classic IRT is visible but marked unsupported. Mesoscopic settings changed while the stage is off are a non-blocking warning. Mesoscopic settings, pH under a Chemistry without an acid-base buffer (BoscoloChem, Farokhi2023) and primary-electron energy cuts with protons are inert: the controls are disabled with the reason, and a value changed before the conflict arose is reported with explicit fixes instead of being dropped or changed silently.
- Beam: electrons above 1 MeV are refused (the Geant4-DNA electron models of `G4EmDNAPhysics_option2` stop at 1 MeV). The beam always moves along +z from (0, 0, z); z (default 0, the box centre) must be strictly inside the box and is written as `/gun/position 0 0 z` when not 0. The primary-electron energy cuts can be left at the code defaults, customised, or turned off; dnachem-min has no off switch yet, so Off sets both thresholds to 10 MeV, which a primary electron of at most 1 MeV can never lose (plan for a real switch: `.claude/plans/2026-11-03-primary-killer-energy-cuts-switch.md`).
- Problems are labelled Incomplete, Invalid, Incompatible or Unsupported and block export; warnings and notes do not. A valid configuration is only a configuration check. It does not mean the simulation was run.
- Not offered, deliberately: scavengers other than O2 (no Chemistry has bulk reactions for them), `/primaryKiller/minKineticE` (no handler), the Kreipl2009 and Meesungnoen2002_amorphous solvation models (accepted by Geant4 but not exercised by this project), thread count (`--threads` on the command line), gun x/y position and direction (the beam stays on the central z axis, along +z).

Export: the review step sanitises the file name (letters, digits, `.`, `_`, `-`, always ending in `.in`). **Download** saves exactly the previewed text to the browser's download folder. Where the browser offers a save dialog (`showSaveFilePicker`), **Save to a folder…** lets the reader pick the folder, normally `macro/`. A web page cannot write into the repository on its own. Saved macros belong in `macro/`; the build copies that folder next to the executable, so after rebuilding run `./sim <name>.in` from the build directory. The configuration is kept for the browser tab session (`sessionStorage`); **Reset all settings…** asks before clearing it.

After editing:

```bash
node docs/explorer/build.js            # regenerate index.html
node docs/explorer/check.js            # validate
node docs/explorer/macro-builder.test.js   # builder model tests
```

## What the check enforces

`check.js` also runs as the `ExplorerCheck` ctest test. It fails when a linked path does not exist, when a file under `src/` or `header/` is not listed in the JSON, when a recipe in `docs/recipes/` or an ADR in `docs/adr/` is missing from the Tasks view, when an edge, phase or `startHere` entry names an unknown id, when `phases`, `catalog`, `commands[].builder` or `parts` are invalid, or when `index.html` is out of date. In detail, for the catalogue:

- the phases are exactly the six ids `setup`, `physical`, `prechem`, `hetero`, `meso`, `output`, in order, each with window text; a step's `catalog` is `physics`, `prechem` or `reactions`; each `commands[].cmd` starts with `/`; a `commands[].builder`, when present, is a Macro builder `STEPS` id; each `parts` entry is a Module map node id;
- the Chemistries in `catalog.json`, in `macro-builder.js` `CHEMISTRIES` and registered in `src/chemistry/BuiltInChemistries.cc` are the same set (`CATALOG_PATH=<file>` and `BUILTIN_PATH=<file>` point the check at other files, for testing);
- every Chemistry in `catalog.json` has `timeStepModel` `IRT_syn`; every reaction and bulk reaction has `order` 1 or 2 and a finite number under its key (`k_s-1` or `k_M-1s-1`); every reaction has an integer `type`; every reactant, product, bulk partner and bulk product is one of that Chemistry's `molecules`;
- every row of `physics-catalog.json` has a non-empty `ref`, and the four time windows have text.

Whether `catalog.json` still matches the live chemistry is not checked here (it needs a built `sim`); `CatalogExportCheck` does that.

It also checks box roles (present, at most 40 characters), step numbering, the chemistry-mode data (one mode step in the first phase, unique branch ids, one default, `modes` keys that name a branch), run overviews, embedded JavaScript syntax and the absence of external assets or runtime network loaders (an inline `data:` URI, such as the favicon built from `docs/assets/dnachem-min-icon.svg`, is allowed). These structural offline checks complement browser inspection over a temporary loopback server; they do not constitute a browser test of `file://`.

Needs Node only for rebuilding and checking, not for reading.
