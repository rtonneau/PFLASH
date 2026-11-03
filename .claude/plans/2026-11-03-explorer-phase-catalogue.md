# Explorer phase catalogue Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The explorer shows, phase by phase, what a run contains (physics processes, pre-chemical channels, full Chemistry reaction tables with k and type for IRT_syn and SBS) with chemistry numbers exported from the running code.

**Architecture:** A new PreInit command `/chem/reaction/exportJson <file>` writes the live chemistry tables (reactions, bulk, molecules, dissociation) as JSON via `DataNode`/`JsonWriter`. A Node script runs `sim` once per Chemistry and merges the results into `docs/explorer/catalog.json`. A hand-written `physics-catalog.json` covers physics, solvation and displacement. `build.js` inlines both, plus a testable view-model module `catalog-view.js`, into `index.html`. The Run flow is regrouped into phases that render the catalogue.

**Tech Stack:** C++20 / Geant4 11.4.1, CMake + CTest, Node (no npm dependencies), plain HTML/JS template.

**Spec:** `.scratch/explorer-phase-catalogue/spec.md`

## Global Constraints

- `index.html` is generated (`node docs/explorer/build.js`). It works from `file://` and GitHub Pages with no external assets or network loaders (`check.js`).
- `ExplorerCheck`, `MacroBuilderTest` and every existing ctest keep passing. Unit tests are built and run from `build-ninja/` (Debug), per `.claude/geant4-instructions.md` section 5. Prepend vcpkg's `debug\bin` to PATH before running ctest (memory: ctest HDF5 debug DLL hang).
- Every physics or chemistry statement is verified in the Geant4 11.4.1 `.cc` source (`C:\DEV\GEANT4\geant4-v11.4.1-source\geant4-v11.4.1\source`) or the project source. If something can't be verified, write `"review": true` and no claim.
- C++: Geant4 style, PascalCase files, camelCase variables, includes rooted at `header/`, `DnaLogger` for logging. Every new `src/`/`header/` file is listed in `architecture.json`.
- Units in exported JSON: k in M^-1 s^-1, D in m^2 s^-1, radius in nm, probability as a fraction 0–1.
- The SBS type column is always 0 by construction (`ChemistryTypes::SetPartialReactionsEnabled(false)`). It is derived in JS, not exported.
- Reaction tables render through one function, used from the Module map, the Macro builder and the Run flow.

## Review Focus

- A Chemistry added to `BuiltInChemistries.cc` without regenerating `catalog.json`: `check.js` must fail, naming the missing Chemistry (test in Task 5).
- A rate edited in a catalog `.cc` without regenerating: `CatalogExportCheck` must fail, naming the reaction and both k values (Task 2).
- Product order or bulk-map order changing between runs, causing spurious diffs: the export sorts bulk entries and keeps table order for bimolecular reactions, and the check compares the same way (Task 1 test, Task 2).
- Species names with the degree-sign bytes (`°OH`, `HO2°`): the export writes them as UTF-8, the species filter in the view matches `OH` typed without the degree sign (Task 4 test).
- `sim` missing or failing during `export-catalog.js --check`: the check exits non-zero with the sim stderr tail, not a silent pass (Task 2).

---

### Task 1: Catalogue export command (C++)

**Files:**
- Create: `header/chemistry/CatalogExport.hh`, `src/chemistry/CatalogExport.cc`
- Modify: `header/chemistry/DnaChemistryList.hh` (member `G4String fCatalogExportFile;`), `src/chemistry/DnaChemistryList.cc` (declare the command next to `dump`, around lines 158-162; write it next to the dump, around line 474)
- Modify: `CMakeLists.txt` (add `CatalogExportTest` after `ReactionTableDumpTest`, same link pattern plus `src/scoring/DataNode.cc src/scoring/JsonWriter.cc`)
- Test: `test/CatalogExportTest.cc`

**Interfaces:**
- Produces (namespace `CatalogExport`):
  - `DataNode MakeReactionNode(const std::string& reactant1, const std::string& reactant2, const std::vector<std::string>& products, double k_M_inv_s_inv, int type);` → object `{"reactants":[r1,r2],"products":[...],"k_M-1s-1":k,"type":type}`
  - `DataNode MakeBulkNode(const std::string& reactant, const std::string& partner, const std::vector<std::string>& products, double k_M_inv_s_inv);` → `{"reactant","partner","products","k_M-1s-1"}`
  - `DataNode MakeMoleculeNode(const std::string& name, double diffusion_m2_s, double radius_nm, int charge);` → `{"name","D_m2s-1","radius_nm","charge"}`
  - `DataNode BuildCatalog(const std::string& chemistryName);` (needs live tables) → `{"chemistry","timeStepModel","geant4Version","molecules":[...],"dissociation":[{"state","channels":[{"name","probability","products":[...],"displacementType"}]}],"reactions":[...],"bulk":[...]}`
  - `void WriteCatalog(const G4String& filename, const std::string& chemistryName);`. A failed open is a fatal `G4Exception` "CannotOpenFile", like `DumpReactionTable`. An Info line goes through `DnaLogger`.
- Macro command: `/chem/reaction/exportJson <file>`, PreInit (same pattern as `/chem/reaction/dump`; the spec's "Idle" is replaced because both tables are only final at that point in `ConstructTimeStepModel`/`CheckTimeStepModel`). Written once from the master/only thread through `OutputDir::Resolve`.

- [ ] **Step 1: Write the failing test** `test/CatalogExportTest.cc` (plain `assert`, no Geant4 runtime):
  - `MakeReactionNode("e_aq","\xC2\xB0OH",{"OHm"},2.95e10,1)` serialised with `JsonWriter::Write` contains `"type": 1`, `"k_M-1s-1": 29500000000`, and the UTF-8 bytes of `°OH`.
  - `MakeReactionNode("H3Op","OHm",{},1.43e11,0)` has `"products": []`.
  - `MakeBulkNode("e_aq","O2",{"O2m"},1.74e10)` has keys in the order reactant, partner, products, k.
  - `MakeMoleculeNode("H2O2",2.3e-9,0.21,0)` has keys name, D_m2s-1, radius_nm, charge.
- [ ] **Step 2: Run it to verify it fails.** `cmake --build build-ninja --target CatalogExportTest`. Expected: compile error, `CatalogExport.hh` not found.
- [ ] **Step 3: Implement the four `Make*` functions and `BuildCatalog`/`WriteCatalog`.**
  - `BuildCatalog` walks `G4DNAMolecularReactionTable::GetVectorOfReactionData()` (table order, `GetReactionType()`, `GetObservedReactionRateConstant()/(1e-3*m3/(mole*s))`).
  - For bulk it reuses `ScavengerReactionAccess::GetReactionMap()`, iterating molecules exactly as `ReactionTableDump::WriteBulkReactions` does, sorted by (reactant, partner, products).
  - For molecules it uses `G4MoleculeTable::GetConfigurationIterator()` with `GetDiffusionCoefficient()/(m2/s)`, `GetVanDerVaalsRadius()/nm` and `GetCharge()`, sorted by name.
  - For dissociation it calls `conf->GetDefinition()->GetDecayChannels(conf)` (skips null), recording `GetName`, `GetProbability`, the product names and `GetDisplacementType`.
  - `timeStepModel` comes from `G4EmParameters::Instance()->GetTimeStepModel()` as `"IRT_syn"`/`"SBS"`, and `geant4Version` from `G4Version.hh`'s `G4VERSION_NUMBER` formatted `11.4.1`.
- [ ] **Step 4: Run the test** from `build-ninja/`: `ctest -R CatalogExportTest --output-on-failure`. Expected: PASS.
- [ ] **Step 5: Smoke the command.** Build `build/` (RelWithDebInfo). From `build/`, run a macro with `/chem/reaction/exportJson cat.json` then `/run/initialize`, with `--dir <tmp>`. Expected: `<tmp>/cat.json` exists; PureWater's reaction count equals the number of `add`/`partial` calls in `BuildPureWaterReactions` (count them; do not assume a number), at least one `"type": 1`, a `bulk` entry `e_aq` + `O2`, and non-empty `dissociation`.
- [ ] **Step 6: Commit** `feat(chemistry): /chem/reaction/exportJson writes the live chemistry catalogue`.

### Task 2: Generator script, committed catalogue, drift ctest

**Files:**
- Create: `docs/explorer/export-catalog.js`, `docs/explorer/catalog.json` (generated)
- Modify: `CMakeLists.txt` (inside `if(NODE_EXECUTABLE)`: `add_test(NAME CatalogExportCheck COMMAND ${NODE_EXECUTABLE} .../export-catalog.js --check --sim $<TARGET_FILE:sim>)`)

**Interfaces:**
- Consumes: `/chem/reaction/exportJson` (Task 1), `/chem/list` names via `macro-builder.js` `CHEMISTRIES` (`require('./macro-builder.js').CHEMISTRIES`)
- Produces: `catalog.json` = `{"generatedBy":"docs/explorer/export-catalog.js","geant4Version":"11.4.1","chemistries":{"<Name>":<BuildCatalog object without its chemistry key>}}`, keys in the `CHEMISTRIES` order.
- CLI: `node export-catalog.js --sim <path-to-sim> [--check]`. Without `--check` it writes `catalog.json`. With `--check` it compares against `catalog.json`: same structure and strings, numbers equal within relative 1e-9; it exits 1 and prints each differing path (e.g. `PureWater.reactions[4].k_M-1s-1: 2.95e10 vs 3.0e10`).
- Each Chemistry runs in its own temp dir: macro `/chem/select <Name>`, `/chem/reaction/exportJson cat.json`, `/run/initialize`; `sim <macro> --dir <tmp>`. A non-zero exit or a missing `cat.json` is an error that prints the last 20 lines of the sim output.

- [ ] **Step 1: Write the comparison as an exported pure function** `diffCatalog(a, b) -> string[]` and a test file `docs/explorer/export-catalog.test.js`:
  - identical objects → `[]`
  - k differing by 1e-12 relative → `[]`
  - k 2.95e10 vs 3.0e10 → one entry containing `reactions[0]`
  - a missing Chemistry key → an entry naming it
  - array length differences → an entry
- [ ] **Step 2: Run** `node docs/explorer/export-catalog.test.js`. Expected: FAIL (module missing).
- [ ] **Step 3: Implement `export-catalog.js`** (`diffCatalog`, the runner, the CLI). Register `export-catalog.test.js` as ctest `CatalogExportScriptTest` beside `MacroBuilderTest`.
- [ ] **Step 4: Run the tests.** Expected: PASS.
- [ ] **Step 5: Generate.** `node docs/explorer/export-catalog.js --sim build/RelWithDebInfo/sim.exe` (adjust to the actual `build/` binary path). Expected: `catalog.json` with 4 Chemistries; `HO3` among the Tonneau2025 molecules only; Tonneau2025 has 73 reactions and BoscoloChem has 26 (otherwise investigate before going on: a pair shared with a bulk reaction can merge entries; record what you find in the spec's Comments).
- [ ] **Step 6: Run** `ctest -R CatalogExport --output-on-failure` from `build-ninja/` (Debug sim). Expected: PASS. If the Debug sim cannot initialise there (data env, DLLs), fix the test's `ENVIRONMENT` property rather than skipping it, and report what was needed.
- [ ] **Step 7: Commit** `feat(explorer): generate catalog.json from the live chemistry, with drift check`.

### Task 3: Hand-written physics catalogue

**Files:**
- Create: `docs/explorer/physics-catalog.json`

**Interfaces:**
- Produces:
  ```
  { "physicsConstructor": "G4EmDNAPhysics_option2",
    "particles": { "e-": [ {"process","model","eMin","eMax","note"?,"ref","review"?} ], "proton": [ ... ] },
    "protonSecondaries": [ same row shape, only the particles a proton beam produces that option2 tracks ],
    "solvation": { "default": {"model","what","ref"}, "alternatives": [ {"model","macroValue","what","ref"} ] },
    "displacement": { "class": "G4DNAWaterDissociationDisplacer", "what", "rows": [ {"case","rms_nm"?,"what","ref"} ], "ref" },
    "meso": { "partialPairs": "<verified sentence or empty>", "reactionsRecorded": false, "ref", "review"? },
    "timeWindows": { "physical","prechem","hetero","meso": {"text","ref"} } }
  ```
  Energies are strings with units (`"11 eV"`, `"1 MeV"`). `ref` is a path under `G4:` or the project.

- [ ] **Step 1: Read** `G4EmDNAPhysics_option2.cc` and `G4EmDNABuilder.cc` (whichever builds option2 in 11.4.1). For every process attached to e- and proton, record the model class and the low/high limits set there, falling back to the model's own defaults in its `.cc` when the constructor sets none.
- [ ] **Step 2: Read** `G4DNAOneStepThermalizationModel.cc` (default Meesungnoen2002; the Macro builder's solvation options in `macro-builder.js` `solvation` are the alternatives offered) and `G4DNAWaterDissociationDisplacer.cc` (the RMS values per displacement type).
- [ ] **Step 3: Read** `G4DNAEventScheduler.cc`/the mesoscopic Gillespie sources for whether reaction type matters there. Write `meso.partialPairs` only if verified; otherwise leave it empty with `"review": true`.
- [ ] **Step 4: Validate** with `node -e "JSON.parse(require('fs').readFileSync('docs/explorer/physics-catalog.json'))"`. Expected: no error. Every row has a `ref`.
- [ ] **Step 5: Commit** `docs(explorer): hand-written physics, solvation and displacement catalogue with G4 refs`.

### Task 4: Catalogue view model (`catalog-view.js`)

**Files:**
- Create: `docs/explorer/catalog-view.js` (UMD like `macro-builder.js`: `module.exports` under Node, `window.CatalogView` in the page), `docs/explorer/catalog-view.test.js`
- Modify: `CMakeLists.txt` (ctest `CatalogViewTest`)

**Interfaces:**
- Consumes: `catalog.json` (Task 2), `physics-catalog.json` (Task 3) as plain objects
- Produces:
  - `reactionRows(catalog, chemistry) -> [{label, reactants, products, k, kText, typeIrt, typeSbs: 0}]`, where `label` = `"A + B → C + D"` (`"(no products)"` when empty) and `kText` = `"2.95 × 10^10"` style with 3 significant digits
  - `bulkRows(catalog, chemistry) -> [{label, partner, k, kText, driver}]`, where `driver` = `"/chem/env/scavenger O2"` for partner `O2`, `"/chem/env/pH"` for `H3Op(B)`/`OHm(B)`, else `null`
  - `moleculeRows(catalog, chemistry) -> [{name, D, radius_nm, charge, extra}]`, where `extra` is true when the molecule is absent from `PureWater`
  - `dissociationRows(catalog, chemistry) -> [{state, name, probabilityText, products}]`
  - `physicsRows(physics, particle) -> rows` (`particle` ∈ `"e-"`, `"proton"`; proton includes `protonSecondaries`)
  - `filterRows(rows, query) -> rows`, case-insensitive on reactants/products/label, ignoring the `°` sign in both the query and the names
  - `chemistryNames(catalog) -> string[]`

- [ ] **Step 1: Write failing tests** in `catalog-view.test.js` against a small inline fixture plus the real `catalog.json`:
  - `reactionRows(real,'PureWater')` has the same length as `real.chemistries.PureWater.reactions`, and every `typeSbs === 0`
  - the `e_aq + °OH` row has `typeIrt === 1`
  - `bulkRows(real,'PureWater')` has a row with `driver === '/chem/env/scavenger O2'`
  - `bulkRows(real,'BoscoloChem')` has no `/chem/env/pH` driver
  - `moleculeRows(real,'Tonneau2025')` has `HO3` with `extra === true`
  - `filterRows(rows,'oh')` keeps rows containing `°OH`
  - `kText(1.43e11) === '1.43 × 10^11'`
  - `chemistryNames(real)` equals `require('./macro-builder.js').CHEMISTRIES.map(c => c.value)`
- [ ] **Step 2: Run** `node docs/explorer/catalog-view.test.js`. Expected: FAIL.
- [ ] **Step 3: Implement the functions.** No DOM code in this file.
- [ ] **Step 4: Run.** Expected: PASS.
- [ ] **Step 5: Commit** `feat(explorer): catalogue view model with tests`.

### Task 5: Build, data model and checks

**Files:**
- Modify: `docs/explorer/build.js` (new markers `/*__CATALOG_JSON__*/null`, `/*__PHYSICS_JSON__*/null`, `/*__CATALOG_VIEW_JS__*/`, same escaping and `</script` guard as now)
- Modify: `docs/explorer/architecture.json` (`phases`, `flow`)
- Modify: `docs/explorer/check.js`
- Modify: `docs/explorer/template.html` (only add the three markers, as `const CATALOG = ...; const PHYSICS = ...;` and the inlined module)

**Interfaces:**
- `phases[]` ids in order: `setup`, `physical`, `prechem`, `hetero`, `meso`, `output`, each gaining `"window"` (text from `physics-catalog.json` `timeWindows`, copied, not referenced).
- Flow steps (my call on the spec's open split; renumber titles `N.`):
  1. Start-up (`setup`)
  2. Chemistry mode (`setup`)
  3. Physical stage (`physical`, `catalog: "physics"`)
  4. Track lengths recorded (`physical`; the old "End of event". Its data is the physical tracks; the summary states the row is committed at event end for non-aborted events)
  5. Physico-chemical stage (`prechem`, `catalog: "prechem"`; absorbs the old "Physics to chemistry" step; outputs `PreChemical_run<R>_event<E>.txt` (staged); files include `StackingAction.cc`, `EventAction.cc`, `DnaChemistryList.cc`)
  6. Particle-based stage (`hetero`, `catalog: "reactions"`)
  7. Hand-over and mesoscopic stage (`meso`, `catalog: "reactions"`, `modes` meso-only as now)
  8. Dump and manifest (`output`; the summary says it publishes what earlier phases recorded or staged)
- New optional step fields: `"catalog": "physics"|"prechem"|"reactions"`, `"commands": [{"cmd": "/chem/meso/handOverTime", "builder": "<STEPS id>"}]`, `"parts": ["<node id>", ...]`. Outputs keep their current `outputs` field; each output string ends with `(staged)`, `(recorded)` or `(written at dump)`.
- `check.js` additions:
  - `phases` contains exactly the six ids above
  - `catalog` values are valid
  - `commands[].builder` is an id in `require('./macro-builder.js').STEPS`
  - `parts` are node ids
  - `catalog.json` has exactly the Chemistries of `macro-builder.js` `CHEMISTRIES`, and every reaction has a numeric k and an integer type (this is the "Chemistry added without regenerating" guard)
  - `physics-catalog.json` rows each have `ref`
  - `index.html` freshness still uses `build.render()`

- [ ] **Step 1: Write the failing check.** Add the rules to `check.js`, then temporarily delete `BoscoloChem` from a copy of `catalog.json` passed through `CATALOG_PATH` env (support that override in `check.js` for this purpose). Run `CATALOG_PATH=<copy> node docs/explorer/check.js`. Expected: FAIL naming `BoscoloChem`.
- [ ] **Step 2: Update `architecture.json`** to the phases and steps above, with `commands` taken from the messengers (verify each command exists in source) and `parts` from the current boxes.
- [ ] **Step 3: Update `build.js`** and add the markers to `template.html`.
- [ ] **Step 4: Run** `node docs/explorer/build.js && node docs/explorer/check.js && node docs/explorer/macro-builder.test.js`. Expected: all pass. The page still renders (flow cards show the new phases; catalogue sections come in Task 6).
- [ ] **Step 5: Commit** `feat(explorer): regroup run flow into simulation phases; inline catalogues`.

### Task 6: Run flow rendering (phases, choosers, catalogue sections)

**Files:**
- Modify: `docs/explorer/template.html` (`modePicker` ~L1119, `applyFlowMode` ~L1136, `renderFlow` ~L1151, `selectFlow` ~L1186, CSS)

**Interfaces:**
- Consumes: `CatalogView.*` (Task 4), step fields (Task 5)
- Produces (used by Task 7):
  - `reactionTableEl(chemistry, opts) -> HTMLElement`, `opts = {highlight: 'irt'|'sbs'|null, filter: true}`. It renders the reaction table (columns: Reaction, k (M⁻¹ s⁻¹), Type under IRT_syn, Type under SBS), a note on the SBS column ("SBS keeps every reaction type 0: partially diffusion-controlled pairs are only type 1 under IRT_syn"), then a "Bulk reactions" table (Reaction, k, Set by) and a species filter box (labelled input, `aria-controls` on the tables, live row count).
  - `prechemEl(chemistry)`, `physicsEl(particle)`: same table style.
- Flow state: `flowChemistry` (default `PureWater`) and `flowParticle` (default `e-`), stored with the existing mode in `sessionStorage` under the same key scheme the mode uses. The Chemistry chooser renders beside the mode chooser (same radio-group markup as `modePicker`). The particle chooser renders only inside the physical stage detail.
- Step detail sections, in this order after the summary: Time window, What can happen (catalogue by `catalog`; for `reactions` with the meso step also show `physics-catalog.json` `meso` sentences), Macro commands (each a link to `#builder/<builder>`), Outputs, Module map parts (`nodeLinks(parts)`), then the existing Code/Docs. The type column for the current mode is highlighted (`irt` for irt/meso, `sbs` for sbs).

- [ ] **Step 1: Implement the choosers and sections.**
- [ ] **Step 2: Build and check:** `node docs/explorer/build.js && node docs/explorer/check.js`. Expected: PASS.
- [ ] **Step 3: Browser verification** (serve `docs/` over a loopback server, as earlier explorer passes did). Verify each of these:
  - every phase renders
  - switching mode to SBS greys out the mesoscopic step and moves the highlight to the SBS column
  - switching to Tonneau2025 shows HO3 in the physico-chemical molecules and 73 rows
  - typing `oh` filters
  - the proton chooser changes the physics table
  - at narrow width (≤ 600 px) the tables scroll horizontally inside their container, not the page
  - no console errors
  - also open `index.html` via `file://` once
- [ ] **Step 4: Commit** `feat(explorer): run-flow phases show processes, reactions, commands and outputs`.

### Task 7: Module map Chemistry boxes and Macro builder access

**Files:**
- Modify: `docs/explorer/template.html` (`showNode` ~L1065, builder `renderField` ~L1560)
- Modify: `docs/explorer/architecture.json` (summaries of `pure-water`, `boscolo`, `tonneau`, `farokhi`: reaction count and bulk-reaction count taken from `catalog.json`; mention that the full table is below)
- Modify: `docs/explorer/macro-builder.test.js` only if the `CHEMISTRIES` shape changes (it should not)

**Interfaces:**
- Consumes: `reactionTableEl` (Task 6)
- A node is a Chemistry box when `CHEMISTRIES.some(c => c.node === n.id)`. Its detail gets a "Reactions" section with `reactionTableEl(c.value, {highlight: null, filter: true})`, placed after the summary.
- Builder Chemistry field: under the selected option, a `<details>` "Show the N reactions of <Name>" containing `reactionTableEl(cfg.chemistry, {highlight: cfg.model === 'SBS' ? 'sbs' : 'irt', filter: true})`. The existing "full description →" link stays.

- [ ] **Step 1: Implement both.**
- [ ] **Step 2: Build, check, run the builder tests.** Expected: all pass.
- [ ] **Step 3: Browser verification:**
  - `#map/boscolo` shows 26 rows and no pH-driven bulk row
  - in the builder, selecting Farokhi2023 updates the `<details>` count and choosing SBS moves the highlight
  - keyboard: the filter and `<details>` are reachable and Escape behaviour is unchanged
- [ ] **Step 4: Commit** `feat(explorer): full reaction tables in Chemistry boxes and the Macro builder`.

### Task 8: Documentation

**Files:**
- Modify: `docs/explorer/README.md`:
  - new phases and step fields (`catalog`, `commands`, `parts`, `window`)
  - the Chemistry/particle choosers
  - the catalogue files and their sources
  - the regeneration command `node docs/explorer/export-catalog.js --sim <build>/sim`, to run after any catalog `.cc` change
  - `CatalogExportCheck`
  - the new `check.js` rules
- Modify: `CLAUDE.md`: add `/chem/reaction/exportJson` to the `DnaChemistryList.cc` Key Files entry and Macro section; add `CatalogExport.cc` as a Key File line
- Modify: `docs/explorer/architecture.json`: `CatalogExport.cc/.hh` added to the `chemistry` lane (box or `otherFiles`; a box named "Catalogue export" with an edge to `reaction-dump`'s neighbours is my call)
- Modify: `.scratch/explorer-phase-catalogue/spec.md`: Status to `implemented`, plus a Comments note: command is `/chem/reaction/exportJson`, PreInit, not `/chem/catalog/exportJson` Idle, and why

- [ ] **Step 1: Write the docs.**
- [ ] **Step 2: Run** `node docs/explorer/build.js && node docs/explorer/check.js`, then the full `ctest --output-on-failure` from `build-ninja/`. Expected: all pass. Report any failure with its output.
- [ ] **Step 3: Commit** `docs: explorer phase catalogue and catalogue export`.
