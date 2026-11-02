# Architecture explorer

A clickable guide to the code for newcomers and non-coders: module map, run flow and task guides.

## Open it

Double-click `docs/explorer/index.html` (or open it in any browser). It works offline and needs no server.

## Edit it

`architecture.json` is the only source of content; `index.html` is generated from it and `template.html`.

- `lanes[]`: one per source folder, with `nodes[]` (title, a short plain-language `role` shown under the title in the box, at most 40 characters, the fuller `summary`, `files`, `symbols`) and `otherFiles[]` for files that are not worth a box.
- `edges[]`: `{from, to}` node ids; `from` uses, creates or fills `to`. The map draws only the selected box's edges (solid arrows: what it uses; dashed: what uses it) and tags the related boxes "used by selected" or "uses selected"; narrow screens show the tags without arrows.
- `phases[]`: `{id, title, summary}`, the groups of the run flow (start-up, per event, output).
- `flow[]`: the run steps, titled `"N. Title"` in order, each with a short `overview` for scanning, the full `summary` for details, a `phase`, an optional `condition` (when the step only happens in some modes), output file names and linked docs. Keep scientific qualifications in the summary, condition and mode notes; verify changes against source.
- Chemistry modes: one step (the chemistry mode, in the first phase because it is chosen before `/run/initialize`) has `branches[]` `{id, label, summary, default?}`; the run flow shows a chooser under it. A later step may have `modes` `{branchId: note}`: the note is shown on its card for that mode, and a mode missing from `modes` marks the step as skipped. Steps without `modes` run the same way in every mode.
- `tasks[]`: entry points that link the guides in `docs/recipes/` and the ADRs.
- `startHere`: `{map, flow, tasks}`, each a list of `{id, why}` shown above the view as suggested first clicks, with the plain-language reason first.
- `"review": true` marks a summary that still awaits a physics review (shown with a dashed outline and a ⚑ marker).

The page derives the rest from the file lists: a box's run steps and tasks are those that share one of its files, so keep `files` accurate. The map's Find box searches titles, roles, summaries, file names and symbols. Matching parts stay grouped by folder; Clear search or Escape restores the full map. Following a connection to a filtered-out part clears the search so the destination is visible; Back restores the previous search. Links such as `index.html#map/time-step` or `#flow/dump` open a view with an item selected.

The detail panel's **Back to …** action follows browser history. During the current page session it restores the search, scroll position and keyboard focus. Tabs remember selections and retain the map search. Each view has a visible introduction and suggested starting points on desktop and narrow screens. On narrow screens, selection opens a dedicated detail view; **Back to Module map / Run flow / Tasks** (or Escape) returns to the selected item. Arrow keys switch tabs; Enter opens the first search match. The keyboard skip link goes straight to the explorer. Copy-path buttons announce success or failure to screen readers. The chosen chemistry mode is kept while the page is open. Full run explanations, per-mode comparisons and output names live in details.

After editing:

```bash
node docs/explorer/build.js   # regenerate index.html
node docs/explorer/check.js   # validate
```

## What the check enforces

`check.js` also runs as the `ExplorerCheck` ctest test. It fails when a linked path does not exist, when a file under `src/` or `header/` is not listed in the JSON, when a recipe in `docs/recipes/` or an ADR in `docs/adr/` is missing from the Tasks view, when an edge, phase or `startHere` entry names an unknown id, or when `index.html` is out of date.

It also checks box roles (present, at most 40 characters), step numbering, the chemistry-mode data (one mode step in the first phase, unique branch ids, one default, `modes` keys that name a branch), run overviews, embedded JavaScript syntax and the absence of external assets or runtime network loaders. These structural offline checks complement browser inspection over a temporary loopback server; they do not constitute a browser test of `file://`.

Needs Node only for rebuilding and checking, not for reading.
