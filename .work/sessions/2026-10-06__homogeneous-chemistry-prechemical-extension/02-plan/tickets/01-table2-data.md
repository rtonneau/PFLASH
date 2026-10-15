# Ticket 01: table2-data

**Model:** sonnet
**Effort:** high

**Acceptance Criteria:**
- [ ] `header/chemistry/catalog/Tonneau2025Table.hh` and `src/chemistry/catalog/Tonneau2025Table.cc` hold all 73 reactions of Table 2 as plain data (reactants, products, k, unit, table row number R1 to R73, source letter A to D), standard library only.
- [ ] R20 to R37 and every multi-value row are checked against the PDF page images (Read tool with `pages`), with any uncertainty listed in the file header.
- [ ] Water pseudo-first-order and equilibrium entries (R1, R23, R58 to R73) are marked so a builder can route them to the bulk list.
- [ ] A new `Tonneau2025TableTest` (plain assert, MSVC CRT guard) checks the count (73), unique ids and a few pinned k values and products, and is registered in `CMakeLists.txt`.

**Files to Touch:**
- `header/chemistry/catalog/Tonneau2025Table.hh`
- `src/chemistry/catalog/Tonneau2025Table.cc`
- `test/Tonneau2025TableTest.cc`
- `CMakeLists.txt`

**Verification Step:**

Run:
```bash
ctest --test-dir build-ninja -R Tonneau2025TableTest --output-on-failure
```

Expected:
`Tonneau2025TableTest` passes (1/1), after building the target `Tonneau2025TableTest` in `build-ninja/` with the MSVC environment.

**Notes:**

The paper is `docs/literature/Tonneau_2025_Phys._Med._Biol._70_235021.pdf`; Table 2 is on PDF page 7 (journal page 6). The reaction text uses a Python-style ODE network, not Geant4 types, so keep species names as in the paper and let ticket 4 map them (e.g. `e-aq` to `e_aq`, `OH.` to the project's OH name, `O2-` to `O2m`). Follow the portable-file convention of `PureWaterReactions.cc` (adjust the include prefix on copy).
