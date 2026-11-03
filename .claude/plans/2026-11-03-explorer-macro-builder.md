# Explorer: Macro Builder (4th view)

## Context

Users write `.in` macros by copying existing ones (55 in `macro/`), which spreads
stale or inert commands and ordering mistakes (PreInit vs post-init). Add a guided
**Macro builder** view to `docs/explorer/` that produces a macro valid for the
current dnachem-min code, with live preview, verified compatibility rules,
validation and export. Keep the explorer offline/double-clickable and every
existing view, link, coverage check, copy button and review flag intact.
On approval, copy this plan to `.claude/plans/2026-11-03-explorer-macro-builder.md`
(project rule).

## Verified facts the builder encodes (source refs go into the model + report)

Path resolution: `~/.claude/CONFIG_RESOLUTION.md` does **not exist**; paths come from
`.claude/.claude-project.json` + env vars. `geant4SourcePath` = 11.4.1 source, but
`G4_ROOT` points to `geant4-v11.4.3-install` (version mismatch → flag in report;
commands read from 11.4.1 source as configured; spot-check the 11.4.3 source for
the same messengers).

| Setting | Command / states / default | Rule (source) |
|---|---|---|
| Logger | `/dnaLogger/verbose Quiet\|Error\|Warning\|Info\|Debug\|Trace`, any state, default Quiet | `DnaLogger.cc:43`, `:15` |
| Console | `/run/verbose 0..4`, `/tracking/verbose >= -1` | G4RunMessenger:98, G4TrackingMessenger:100 |
| Solvation | `/process/dna/e-SolvationSubType` PreInit; omitted → Meesungnoen2002 (fDNAUnknownModel) | G4EmLowEParametersMessenger:159, G4DNAOneStepThermalizationModel.cc:291. Offer Ritchie1994 / Meesungnoen2002 / Terrisol1990 (used by project macros); Kreipl2009, Meesungnoen2002_amorphous omitted + documented (never exercised here) |
| Chemistry | `/chem/select` PreInit; PureWater (default), BoscoloChem, Tonneau2025, Farokhi2023 | `ChemistrySelectMessenger.cc`, `BuiltInChemistries.cc` |
| Time-step model | `/process/chem/TimeStepModel` PreInit: IRT_syn (default), SBS; **IRT shown, unselectable: unsupported** (fatal `UnsupportedTimeStepModel`) | `DnaChemistryList.cc:318` |
| Meso stage | `/chem/meso/enable` PreInit, default true; `true`+SBS fatal (`MesoWithSbs`) | `DnaChemistryList.cc:336`, `MesoSettings.cc:75` |
| Meso params | handOverTime >0 (5 ns, default unit ns), voxelSize >0 (6.25 nm), timesPerDecade ≥1 int (10), spatialOutput bool (false); PreInit; only meaningful with meso on | `MesoMessenger.cc` |
| End time | `/scheduler/endTime` (PreInit/Idle) **after** `/run/initialize` (CheckTimeStepModel resets it at init); default 1 s meso / 1 us without | `DnaChemistryList.cc:346`, G4SchedulerMessenger:53 |
| Hand-over ≥ end time | not fatal; meso stage never runs → warning | `TimeStepAction.cc:441,477` |
| Reaction bins | default 7-edge table / `timeBinsFixed <w> <unit>` / `timeBinsList <e..> <unit>`, PreInit, mutually exclusive (fatal) → radio; list units ps/ns/us/ms/s (+long names); fixed: warn with computed bin count (end/width) | `DnaChemistryList.cc:164-235`, `ReactionCounter.cc:39` |
| O2 scavenger | `/chem/env/scavenger O2 <v> M\|mM\|uM\|%`, ≥0, PreInit; all 4 Chemistries have O2 bulk reactions. Other species omitted (would be inert/unknown per Chemistry) | `ScavengerSpec.cc`, catalog bulk lists |
| pH | `/chem/env/pH` PreInit, default 7, no range check; effective only for PureWater, Tonneau2025 (buffer ions); disabled with explanation for BoscoloChem, Farokhi2023 | `PureWaterReactions.cc:156`, `Tonneau2025Reactions.cc:280`, CLAUDE.md |
| Half box | `/chem/env/halfBox <v> nm\|um\|mm` PreInit >0, default 500 um | `DnaChemistryWorld.cc:28,55` |
| Output dir | optional `/run/outputDir <path>` PreInit; note conflict with `--dir` is fatal at runtime | `OutputDirMessenger.cc` |
| Gun | `/gun/particle e-` (default, scope) or `proton` (project Farokhi macros); `/gun/energy >0 eV\|keV\|MeV`; position (0,0,0), direction +z fixed (shown, not editable) | `PrimaryGeneratorAction.cc`, G4ParticleGunMessenger |
| Primary killer | `/primaryKiller/eLossMin`, `eLossMax` (energy units), defaults 500 keV / 1 MeV; commands exist **only after `/run/initialize`** (created in ConstructSDandField); act on primary **e-** only → incompatible with proton; `minKineticE` omitted (no handler) | `PrimaryKiller.cc:43-80`, `DetectorConstruction.cc:107`, G4RunManager.cc:670 |
| Sub-runs | per sub-run: optional `/random/setSeeds a b` (≥2 ints), `/run/beamOn N` (N≥0; builder requires ≥1), then dump: none / `/run/dumpDataAndReset [prefix]` / `/run/dumpDataAndResetToDir <subdir>` (relative, no `..`, non-empty); prefixes and subdirs unique per process (fatal); undumped data → `EndOfRun_` safety-net (info) | G4RunMessenger:60,277, `RunAccumulatorMessenger.cc`, `OutputDir.cc:151` |
| Threads | not a macro choice here: `--threads N` CLI; omitted, documented | `sim.cc:75,130` |
| Smoke preset | `/scheduler/endTime 1 ms`, 10 keV e-, beamOn 2 | CLAUDE.md Testing |

Generated order: header comments → verbosity → PreInit (solvation, chemistry,
time-step model, meso, reaction bins, environment, outputDir) → `/run/initialize`
→ `/scheduler/endTime` → gun → primaryKiller → sub-runs (seeds, beamOn, dump).
Defaults policy: core choices (chemistry, model, meso enable, gun, beamOn) always
emitted; optional settings emitted only when changed, each with a comment.

## Design

**One model module** `docs/explorer/macro-builder.js` (pure JS, UMD: `module.exports`
in Node, `window.MacroBuilder` in the page). Contains:
- `SETTINGS` schema: id, step, label, plain help, technical note, source refs,
  required/optional, type, units, default, `availability(cfg)` →
  `{ok}|{ok:false, kind:'incompatible'|'unsupported', why, fix}`.
- `validate(cfg)` → issues `{id, field, severity:'error'|'warning'|'info',
  kind:'incomplete'|'invalid'|'incompatible'|'unsupported', message, resolutions:[{label, patch}]}`.
- `generate(cfg)` → `{text, lines}` in the order above.
- `applyPatch(cfg, patch)` (explicit resolutions only; never auto-changes),
  `defaults()`, `sanitizeFilename(name)` (→ `[A-Za-z0-9._-]`, forced `.in`).
Guide, validation markers and preview all read from this module → no drift.

**Build**: `build.js` inlines `macro-builder.js` into `template.html` at a new
marker (`/*__MACRO_BUILDER_JS__*/`), so `index.html` stays self-contained;
`check.js` freshness/offline checks keep applying unchanged (plus: marker present).

**UI in `template.html`** (reuse `el`, `section`, `.card`, `.link`, `.tag`,
`.badge`, `.mode-pick` radio pills, `copyText`, `#copy-status`, light-theme vars;
the explorer has a single light theme — verify contrast there):
- 4th tab **Macro builder** (`#builder`, `#builder/<step>` deep link; arrow-key
  tabs already generic). `#stage` = guide, `#detail` = live preview (sticky).
- Steps: 1 Basics & logging · 2 Chemistry model · 3 Mesoscopic & timing ·
  4 Environment · 5 Beam · 6 Runs & output · 7 Review & export. Stepper
  (`<ol>` with aria-current, per-step status text "3 issues"/"done"), Back/Next,
  free step jumping, state kept in memory (and `sessionStorage` if available,
  guarded).
- Each control: label, Required/Optional tag, plain help, "Technical details"
  `<details>` with command + source refs. Unavailable options stay visible,
  `disabled`, with text "Unavailable: <why>. To enable: <fix>" (text + icon, not
  color only; `aria-describedby`).
- Conflict banners near the control and in Review, with explicit resolution
  buttons (e.g. SBS + meso on → "Turn mesoscopic stage off" / "Switch back to
  IRT_syn"; meso off with custom meso params → "Discard mesoscopic settings" /
  "Re-enable"; proton + killer settings → "Remove primary-killer settings" /
  "Switch to e-"; Chemistry without buffer + pH≠7 → "Reset pH" / "Pick a buffered
  Chemistry"). Issues labelled Incomplete / Invalid / Incompatible / Unsupported.
- Preview panel: status line ("Configuration valid — not a simulation run" /
  "N errors block export"), summary list, `<pre>` macro with line highlights for
  errors, Copy button.
- Review step: summary, all issues with "Go to setting" links, filename field
  (sanitized live, shows final name), **Download** (Blob + `a[download]`, exact
  preview text; disabled with reason while errors), **Save to folder…** only if
  `window.showSaveFilePicker` exists (user picker; fallback note), destination
  advice: save into `macro/` (established, `run.macroDir`), rebuild so the
  POST_BUILD step copies it to `build/…/macro`, run `./sim <name>.in` from `build/`;
  explicit note that the page cannot write into the repo by itself.
- Reset with confirm dialog (`<dialog>` or `confirm`-free inline confirm step).
- Narrow (<900px): guide and preview stacked with a sticky "Guide | Preview
  (status)" switch bar; desktop side by side. Existing narrow `inspecting`
  logic bypassed for this view only.

**Docs**: `docs/explorer/README.md` section (model file, how to update rules when
a messenger changes, tests). `architecture.json` (curated content) unchanged;
`CLAUDE.md` unchanged.

## Tests

`docs/explorer/macro-builder.test.js` (plain `node:assert`, no deps) + CTest
`MacroBuilderTest` (inside the existing `if(NODE_EXECUTABLE)` block). Cases:
default config valid and byte-exact expected macro; ordering (all PreInit before
`/run/initialize`, endTime/gun/killer after, beamOn last per sub-run); SBS+meso
→ incompatible with resolutions, applying "meso off" → valid and emits
`enable false`; IRT unselectable/unsupported; meso params inert when off;
hand-over ≥ end → warning; pH disabled for BoscoloChem; proton + killer conflict;
O2 `%` vs M units, negative → invalid; duplicate prefix/subdir, `..`/absolute
subdir → invalid; bins fixed vs list mutual exclusivity + list parse errors;
missing energy → incomplete; filename sanitizing; export text === preview text;
`applyPatch` never touches unrelated fields.

## Verification

1. `node docs/explorer/build.js`, `node docs/explorer/check.js`,
   `node docs/explorer/macro-builder.test.js`; Debug build-ninja configure + `ctest
   --test-dir build-ninja -R "ExplorerCheck|MacroBuilderTest" --output-on-failure`
   (MSVC env per `.claude/geant4-instructions.md`).
2. Generated macros run with `sim.exe` (RelWithDebInfo `build/`, untracked scratch
   macros in `build/.../macro`, `--dir` to scratch): IRT+meso smoke (1 ms),
   IRT-only, SBS, BoscoloChem+O2, proton 2 events small box; check success
   markers / no `EEEE`/FatalException. Also confirm the builder blocks an
   SBS+meso config (and optionally that sim really fails on it).
3. Browser (headless Chrome over CDP, already verified working on loopback and
   `file://`): desktop 1440×900 and 390×844 screenshots; full workflow by
   keyboard; conflict + resolution; Download (CDP `Browser.setDownloadBehavior`
   to scratch, diff with preview text); other three views still render, deep
   links, copy buttons; console errors none. Direct `file://` run of the builder.
4. Report `.work/sessions/2026-11-03__explorer-macro-builder/report.md`
   (`.work` is gitignored): command-source table, rules, export behaviour,
   verification + limits (no real browser extension, Save-to-folder picker can't
   be exercised headless, version mismatch). Stop temp server, close tabs/Chrome.
No commit/push.
