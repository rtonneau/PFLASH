# Ticket 03: farokhi2023-chemistry

**Model:** sonnet
**Effort:** medium

**Acceptance Criteria:**
- [ ] `header/chemistry/catalog/Farokhi2023Reactions.hh` and `src/chemistry/catalog/Farokhi2023Reactions.cc` exist, following the exact structure of `BoscoloChemReactions.{hh,cc}` (portable: no project-specific includes beyond `chemistry/ChemistryTypes.hh`; local `Conf()` helper via `G4MoleculeTable::Instance()->GetConfiguration(name)`; local UTF-8 `kOH`/`kHO2` tags spelled from explicit bytes).
- [ ] `Farokhi2023Reactions::BuildFarokhiReactions(G4DNAMolecularReactionTable*)` registers exactly the 9 base reactions of Table 2 of Farokhi et al. 2023 (ground truth: `examples/extended/medical/dna/scavenger/Rtable_O2.txt` in the Geant4 11.4.1 source tree, not the paper PDF, which has OCR artifacts), with reaction types per that file's `type_N` labels mapped the same way `PureWaterReactions.cc`/`BoscoloChemReactions.cc` already map Pimblott/LaVerne types to `add()`/`partial()`:
  - `add()` (type 0, fully diffusion-controlled) for `type_1`/`type_3` pairs:
    - `e_aq + e_aq -> H2 + OHm + OHm`, k = 6.36e9 (type_3; H2O reactants/products dropped, as in the existing catalog files)
    - `H3Op + OHm -> {}`, k = 1.13e11 (type_3)
    - `e_aq + H -> H2 + OHm`, k = 2.50e10 (type_1)
    - `H + H -> H2`, k = 5.03e9 (type_1)
  - `partial()` (type 1 under IRT_syn, via `ChemistryTypes::PartialReactionsEnabled()`) for `type_2`/`type_4` pairs:
    - `e_aq + OH -> OHm`, k = 2.95e10 (type_2)
    - `e_aq + H2O2 -> OHm + OH`, k = 1.10e10 (type_2)
    - `OH + OH -> H2O2`, k = 5.50e9 (type_2)
    - `OH + H -> {}`, k = 1.55e10 (type_2)
    - `e_aq + H3Op -> H`, k = 2.11e10 (type_4)
  - No reaction in this function touches `O2`, `Om`, `O3m`, `HO2`, `HO2m`, or `O2m` — unlike `BoscoloChem`, the paper's network never produces O2 as a tracked species; oxygen only ever appears as the bulk/background scavenger below.
- [ ] `Farokhi2023Reactions::BuildFarokhiBulkReactions()` returns exactly 3 `BulkReactionEntry` values (no acid-base buffer, no H2O2/HO2m/O3m equilibria beyond these), matching `Rtable_O2.txt`'s "Reactions with the scavengers" section:
  - `{"e_aq", {{"O2", 1.74e10*M, {"O2m"}, 0}}}`
  - `{"H", {{"O2", 2.10e10*M, {kHO2}, 0}}}`
  - `{"Om", {{"O2", 3.70e9*M, {"O3m"}, 0}}}`
  (same `M = 1e-3 * m3 / (mole * s)` unit convention as `PureWaterReactions.cc`/`BoscoloChemReactions.cc`.)
- [ ] `BuiltInChemistries::Register()` in `src/chemistry/BuiltInChemistries.cc` adds `{"Farokhi2023", &Farokhi2023Reactions::BuildFarokhiReactions, &Farokhi2023Reactions::BuildFarokhiBulkReactions}` (2-argument form, no `constructMolecules` — no new molecules needed), with the matching `#include "chemistry/catalog/Farokhi2023Reactions.hh"` added.
- [ ] `/chem/list` (after `/run/initialize`, or via a quick smoke run) shows `Farokhi2023` alongside `PureWater`, `BoscoloChem`, `Tonneau2025`.
- [ ] CLAUDE.md gets a new bullet in the Key Files section for `src/chemistry/catalog/Farokhi2023Reactions.cc`, in the same style and immediately following the existing `Tonneau2025Reactions.cc` bullet: what paper/table it reproduces, that it has no acid-base buffer, that O2 is bulk-only (never tracked), and the source-of-truth note (`Rtable_O2.txt`, not the PDF).

**Files to Touch:**
- `header/chemistry/catalog/Farokhi2023Reactions.hh`
- `src/chemistry/catalog/Farokhi2023Reactions.cc`
- `src/chemistry/BuiltInChemistries.cc`
- `CLAUDE.md`

**Verification Step:**

Run:
```bash
cmake --build build --config RelWithDebInfo --target sim
cd build
echo -e "/chem/select Farokhi2023\n/chem/list\n/gun/particle e-\n/gun/energy 25 keV\n/run/initialize\n/scheduler/endTime 1 ms\n/run/beamOn 5\n/run/dumpDataAndReset" > macro/scratch_farokhi_smoke.in
./sim.exe scratch_farokhi_smoke.in --dir ../.scratch/tests/2026-10-09__farokhi2023-flash-oxygen-depletion-chemistry-layout-90-mev/ticket03-smoke > ticket03-smoke.log 2>&1
```

Expected: exit 0; log's `/chem/list` output includes `Farokhi2023`; no `EEEE`/`FatalException`; `Manifest.json` in the `--dir` output records `"Chemistry": "Farokhi2023"` (or equivalent key per `RunManifest.cc`); `Species.Txt` is non-empty and shows no `O2`/`O2m`/`HO2`/`O3m` yield above zero at this smoke run's default (anoxic, scavenger unset) condition.

**Notes:**

This is a pure catalog addition; `DnaChemistryList.cc` needs no edit (it reads the selected Chemistry generically via `ChemistryRegistry::Selected()`). Do not reuse `PureWaterReactions::BuildPureWaterReactions()` for the 9 base reactions — despite having the "same 9 pairs", `PureWaterReactions.cc`'s own comment says it uses deliberately different "project values" for 7 of the 9 rates (verified during the grill); only 2 of the 9 numerically coincide (`e_aq+OH` and `e_aq+H3Op`) and that's incidental. The 3 bulk-reaction rates, by contrast, genuinely are identical to `PureWaterReactions::BuildPureWaterBulkReactions()`'s `O2`-partner entries — this is expected (both ultimately come from the same Geant4-DNA UHDR/scavenger-example source), not a bug to "fix" by sharing code; keep `Farokhi2023Reactions.cc` self-contained/portable per the project's established convention.
