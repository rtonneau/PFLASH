# Plan: serve docs/explorer on GitHub Pages

Date: 2026-11-03
Status: awaiting approval

## Context

GitHub Pages is enabled on `rtonneau/dnachem-min`: branch `main`, folder `/docs`
(the only choices GitHub offers are `/` and `/docs`). The explorer will be at
`https://rtonneau.github.io/dnachem-min/explorer/`.

The page is self-contained, so it renders on Pages. Its file links do not work there:

- `template.html:474` sets `const REPO = '../../'`. Links are built as `REPO + path`
  (`template.html:577` file lists, `template.html:1478` linked docs/outputs).
- Only `docs/` is published, so `../../src/...` and `../../header/...` 404.
- By default, Jekyll converts `docs/**/*.md` into `.html`, so links to ADRs/recipes
  (`.md`) 404.

## Goal

The same `index.html` works in both cases:

- opened from disk (`file://`): links stay relative (`../../<path>`), as they are now;
- served over http(s) (Pages, or any server): links open the file in GitHub's
  viewer, `https://github.com/rtonneau/dnachem-min/blob/main/<path>` (GitHub
  redirects `blob/` to `tree/` for directories).

## Changes

1. **`docs/explorer/template.html`** (line 474): replace the constant with
   ```js
   const REPO_WEB = 'https://github.com/rtonneau/dnachem-min/blob/main/';
   const REPO = location.protocol === 'file:' ? '../../' : REPO_WEB;
   ```
   No other call site changes; both link builders already use `REPO + path`.
   Note: local loopback servers used for browser inspection (see README) would
   also get GitHub links. That is acceptable: the links show the committed file,
   not the working copy. Mention it in the README.

2. **`docs/.nojekyll`** (new, empty): Pages serves `docs/` as it is, without Jekyll.
   No `.md` links remain on the site after step 1 anyway; this only saves build time
   and avoids Jekyll surprises (e.g. files starting with `_`).

3. **`docs/explorer/check.js`**: no rule change expected. The external-asset regex
   only matches `src/href` *attributes* in literal `<script|link|img|iframe>` tags and
   `url(//...)` in CSS. The network-loader regex only matches `fetch(`/`XMLHttpRequest(`
   etc. A JS string constant holding a github.com URL triggers neither. Check this by
   running `check.js`; if it does trigger, narrow the rule instead of hiding the URL.
   Optional (include only if you want it): add a check that `REPO_WEB` matches
   `https://github.com/<owner>/<repo>/blob/main/` so a typo is caught.

4. **`node docs/explorer/build.js`**: regenerate `index.html`.

5. **Docs**
   - `docs/explorer/README.md`, section "Open it": add the online URL and say that
     online file links go to GitHub (`main` branch), local ones to the working copy.
   - `README.md`: one line linking the online explorer.
   - `CLAUDE.md`, "Architecture explorer" line: add the online URL.

## Verification

- `node docs/explorer/build.js && node docs/explorer/check.js && node docs/explorer/macro-builder.test.js`
- `ctest -R "ExplorerCheck|MacroBuilderTest"` from `build-ninja/`.
- Local check of both modes:
  - open `docs/explorer/index.html` from disk: a file link points to `../../src/...`;
  - `python -m http.server` in `docs/`, open `/explorer/`: the same link points to
    `https://github.com/rtonneau/dnachem-min/blob/main/src/...`.
- After the user pushes: open `https://rtonneau.github.io/dnachem-min/explorer/`, then
  follow one source link, one ADR link and a `#map/time-step` deep link.

## Out of scope

- Enabling/configuring Pages (done by the user).
- Pinning links to a commit SHA instead of `main` (links show the latest `main`, which
  may differ from the explorer if `index.html` is stale; the `ExplorerCheck` ctest
  keeps them in step).
- Commit/push: only if the user asks.
