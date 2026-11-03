# Architecture explorer

A clickable guide to the code for newcomers and non-coders: module map, run flow, task guides and a macro builder.

## Open it

Double-click `docs/explorer/index.html` (or open it in any browser). It works offline and needs no server.

Online: https://rtonneau.github.io/dnachem-min/explorer/ (GitHub Pages, branch `main`, folder `/docs`; `docs/.nojekyll` turns off Jekyll). Opened from disk, file links go to your working copy (`../../<path>`); served over http(s), including a local test server, they open the file on GitHub at `main`.

## Edit it

`architecture.json` is the only source of content; `index.html` is generated from it and `template.html`.

- `lanes[]`: one per source folder, with `nodes[]` (title, a short plain-language `role` shown under the title in the box, at most 40 characters, the fuller `summary`, `files`, `symbols`) and `otherFiles[]` for files that are not worth a box.
- `edges[]`: `{from, to}` node ids; `from` uses, creates or fills `to`. The map draws only the selected box's edges (teal solid arrows: what it uses; purple dashed arrows: what uses it). Arrowheads always point from the part doing the using to the part it uses. Blue marks selection; amber flags mark summaries awaiting review. These meanings are shared by the map, run flow and task details in the existing light theme.
- `phases[]`: `{id, title, summary}`, the groups of the run flow (start-up, per event, output).
- `flow[]`: the run steps, titled `"N. Title"` in order, each with a short `overview` for scanning, the full `summary` for details, a `phase`, an optional `condition` (when the step only happens in some modes), output file names and linked docs. Keep scientific qualifications in the summary, condition and mode notes; verify changes against source.
- Chemistry modes: one step (the chemistry mode, in the first phase because it is chosen before `/run/initialize`) has `branches[]` `{id, label, summary, default?}`; the run flow shows a chooser under it. A later step may have `modes` `{branchId: note}`: the note is shown on its card for that mode, and a mode missing from `modes` marks the step as skipped. Steps without `modes` run the same way in every mode.
- `tasks[]`: entry points that link the guides in `docs/recipes/` and the ADRs.
- `startHere`: `{map, flow, tasks}`, each a list of `{id, why}` shown above the view as suggested first clicks, with the plain-language reason first.
- `"review": true` marks a summary that still awaits a physics review (shown with a dashed outline and a ⚑ marker).

The page derives the rest from the file lists: a box's run steps and tasks are those that share one of its files, so keep `files` accurate. Selecting a part opens **Connections only** by default: incoming parts sit to its left and outgoing parts to its right, with the source folder shown on every card. Ordered curves stay in the empty gaps between columns. Hover or keyboard-focus a connected part to emphasize its arrow. On narrow screens, labeled groups list the same relationships without long routes. Starting-point suggestions are hidden while focused to leave room for the connections.

Toggle **Connections only** off to return to the full folder map; that choice persists as you select other parts. The full map keeps rounded routes in reserved folder gutters and routes connections across non-adjacent columns above the folders. Starting a search returns to the full searchable map; Back restores the connection view and keyboard focus.

Relationship group headings name the selected part so direction stays clear when its card scrolls out of view. When the explanation sits below the connections, **Read explanation and files** jumps to its return navigation. **Back to connections**, or Escape while focused in that explanation, returns to the selected part without clearing it. Opening a different map selection through its URL brings that part into view.

The map's Find box searches titles, roles, summaries, file names and symbols. Matching parts stay grouped by folder; Clear search or Escape restores the full map. Following a connection to a filtered-out part clears the search so the destination is visible; Back restores the previous search. Links such as `index.html#map/time-step` or `#flow/dump` open a view with an item selected.

The detail panel's **Back to …** action follows browser history. During the current page session it restores the search, scroll position and keyboard focus. Tabs remember selections and retain the map search. Each view has a visible introduction and suggested starting points on desktop and narrow screens. On narrow screens, selection opens a dedicated detail view; **Back to Module map / Run flow / Tasks** (or Escape) returns to the selected item. Arrow keys switch tabs; Enter opens the first search match. The keyboard skip link goes straight to the explorer. Copy-path buttons announce success or failure to screen readers. The chosen chemistry mode is kept while the page is open. Full run explanations, per-mode comparisons and output names live in details.

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

`check.js` also runs as the `ExplorerCheck` ctest test. It fails when a linked path does not exist, when a file under `src/` or `header/` is not listed in the JSON, when a recipe in `docs/recipes/` or an ADR in `docs/adr/` is missing from the Tasks view, when an edge, phase or `startHere` entry names an unknown id, or when `index.html` is out of date.

It also checks box roles (present, at most 40 characters), step numbering, the chemistry-mode data (one mode step in the first phase, unique branch ids, one default, `modes` keys that name a branch), run overviews, embedded JavaScript syntax and the absence of external assets or runtime network loaders (an inline `data:` URI, such as the favicon built from `docs/assets/dnachem-min-icon.svg`, is allowed). These structural offline checks complement browser inspection over a temporary loopback server; they do not constitute a browser test of `file://`.

Needs Node only for rebuilding and checking, not for reading.
