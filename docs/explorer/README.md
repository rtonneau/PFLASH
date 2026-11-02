# Architecture explorer

A clickable guide to the code for newcomers and non-coders: module map, run flow and task guides.

## Open it

Double-click `docs/explorer/index.html` (or open it in any browser). It works offline and needs no server.

## Edit it

`architecture.json` is the only source of content; `index.html` is generated from it and `template.html`.

- `lanes[]`: one per source folder, with `nodes[]` (title, plain-language `summary`, `files`, `symbols`) and `otherFiles[]` for files that are not worth a box.
- `edges[]`: `{from, to}` node ids; `from` uses, creates or fills `to`. The map draws only the selected box's edges (solid arrows: what it uses; dashed: what uses it).
- `phases[]`: `{id, title, summary}`, the groups of the run flow (start-up, per event, output).
- `flow[]`: the run steps, each with a `phase`, an optional `condition` (when the step only happens in some modes), output file names and linked docs.
- `tasks[]`: entry points that link the guides in `docs/recipes/` and the ADRs.
- `startHere`: `{map, flow, tasks}`, each a list of `{id, why}` shown as suggested first clicks while nothing is selected.
- `"review": true` marks a summary that still awaits a physics review (shown with a dashed outline and a ⚑ marker).

The page derives the rest from the file lists: a box's run steps and tasks are those that share one of its files, so keep `files` accurate. The map's Find box searches titles, summaries, file names and symbols. Links such as `index.html#map/time-step` or `#flow/dump` open a view with an item selected.

After editing:

```bash
node docs/explorer/build.js   # regenerate index.html
node docs/explorer/check.js   # validate
```

## What the check enforces

`check.js` also runs as the `ExplorerCheck` ctest test. It fails when a linked path does not exist, when a file under `src/` or `header/` is not listed in the JSON, when a recipe in `docs/recipes/` or an ADR in `docs/adr/` is missing from the Tasks view, when an edge, phase or `startHere` entry names an unknown id, or when `index.html` is out of date.

Needs Node only for rebuilding and checking, not for reading.
