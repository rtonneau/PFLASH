# Architecture explorer

A clickable guide to the code for newcomers and non-coders: module map, run flow and task guides.

## Open it

Double-click `docs/explorer/index.html` (or open it in any browser). It works offline and needs no server.

## Edit it

`architecture.json` is the only source of content; `index.html` is generated from it and `template.html`.

- `lanes[]`: one per source folder, with `nodes[]` (title, plain-language `summary`, `files`, `symbols`) and `otherFiles[]` for files that are not worth a box.
- `edges[]`: `{from, to}` node ids.
- `flow[]`: the run steps, with output file names and linked docs.
- `tasks[]`: entry points that link the guides in `docs/recipes/` and the ADRs.
- `"review": true` marks a summary that still awaits a physics review (shown with a dashed orange outline).

After editing:

```bash
node docs/explorer/build.js   # regenerate index.html
node docs/explorer/check.js   # validate
```

## What the check enforces

`check.js` also runs as the `ExplorerCheck` ctest test. It fails when a linked path does not exist, when a file under `src/` or `header/` is not listed in the JSON, when a recipe in `docs/recipes/` is missing from the Tasks view, or when `index.html` is out of date.

Needs Node only for rebuilding and checking, not for reading.
