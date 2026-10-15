# Ticket 02: rooted-includes

**Acceptance Criteria:**
- [ ] Every project include in `src/`, `header/`, `test/` and `sim.cc` is rooted at `header/` (for example `#include "chemistry/DnaChemistryList.hh"`); the one angle-bracket form in `src/actions/TrackingAction.cc` keeps its delimiter (`#include <actions/TrackingAction.hh>`).
- [ ] `git diff -U0 -- src header test sim.cc` shows only `#include` lines changed; CRLF line endings are preserved.
- [ ] `CMakeLists.txt` no longer loops over header directories; `project_include_dirs` is `${PROJECT_SOURCE_DIR}/header` only.
- [ ] Clean rebuilds of `build` and `build-ninja` succeed; `ctest` reports 7/7 Passed.
- [ ] `verify.sh after_rooted` ends with `STATISTICALLY_COMPARABLE` (within 15% of the mean of the three baselines, quantities with reference count of at least 400).
- [ ] One commit: `refactor: root project includes at header/ and drop per-directory include path`.

**Files to Touch:**
- every `.cc` and `.hh` under `src/`, `header/`, `test/`, plus `sim.cc` (include lines only)
- `CMakeLists.txt` (the include-directory loop)
- `.scratch/tests/2026-09-28__source-layout-subdirs/rewrite-includes.js` (git-ignored)

**Verification Step:**

Run:
```bash
git diff -U0 -- src header test sim.cc | grep -E '^[+-]' | grep -vE '^(\+\+\+|---)' | grep -vE '^[+-]\s*#\s*include\s' || echo "only include lines changed"
grep -n 'include <actions/TrackingAction.hh>' src/actions/TrackingAction.cc
grep -n 'foreach' CMakeLists.txt
bash .scratch/tests/2026-09-28__source-layout-subdirs/verify.sh after_rooted
ctest --test-dir build-ninja --output-on-failure
```

Expected:
`only include lines changed`; the TrackingAction line is found; `foreach` no longer appears in the include-path section (the grep prints nothing); `verify.sh` ends with `STATISTICALLY_COMPARABLE`; ctest reports 7/7 passed. (Run the two clean rebuilds first.)

**Notes:**

1. **Write `$S/rewrite-includes.js`:**
   ```js
   // Roots project includes at header/. Run from the repo root: node rewrite-includes.js
   const fs = require('fs'), path = require('path');
   const walk = d => fs.readdirSync(d, { withFileTypes: true }).flatMap(e =>
     e.isDirectory() ? walk(path.join(d, e.name)) : [path.join(d, e.name)]);
   const rooted = new Map();
   for (const f of walk('header').filter(f => f.endsWith('.hh'))) {
     const base = path.basename(f);
     if (rooted.has(base)) throw new Error('duplicate header name: ' + base);
     rooted.set(base, f.split(path.sep).slice(1).join('/'));
   }
   const files = [...walk('src'), ...walk('header'), ...walk('test'), 'sim.cc']
     .filter(f => /\.(cc|hh)$/.test(f));
   const re = /^(\s*#\s*include\s*)([<"])([A-Za-z0-9_]+\.hh)([>"])/gm;
   let nFiles = 0, nLines = 0;
   for (const f of files) {
     const before = fs.readFileSync(f, 'utf8');
     let n = 0;
     const after = before.replace(re, (m, pre, open, name, close) => {
       if (!rooted.has(name)) return m;
       n++; return pre + open + rooted.get(name) + close;
     });
     if (after !== before) { fs.writeFileSync(f, after); nFiles++; nLines += n; }
   }
   console.log(`rewrote ${nLines} includes in ${nFiles} files`);
   ```
   It substitutes inside a line only, so line endings survive; names not found under `header/` (Geant4 and system headers) are left alone.

2. Run `node $S/rewrite-includes.js` from the repo root; expect one summary line with nonzero counts.

3. **CMake.** In `CMakeLists.txt`, replace the comment plus foreach block (from the "Build include path list…" comment through `list(REMOVE_DUPLICATES project_include_dirs)`) with:
   ```cmake
   # Project headers are included by their path under header/,
   # e.g. #include "chemistry/DnaChemistryList.hh".
   set(project_include_dirs ${PROJECT_SOURCE_DIR}/header)
   ```
   Keep the `headers` glob: `add_executable(sim …)` still uses it. The test targets keep using `${project_include_dirs}`.

4. Clean-rebuild both build directories. A leftover bare project include is now a compile error. If one shows up ("cannot open include file"), the regex missed it (unusual spacing) or it names a header outside `header/`; fix that line by hand and mention it in the commit body.

5. Run the verification commands above, then commit (`git add -A src header test sim.cc CMakeLists.txt`).
