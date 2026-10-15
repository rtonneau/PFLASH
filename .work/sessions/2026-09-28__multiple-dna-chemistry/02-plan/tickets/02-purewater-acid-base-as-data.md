# Ticket 02: purewater-acid-base-as-data

**Acceptance Criteria:**
- [ ] A baseline reaction-table dump and baseline `PhysicsInteractions.csv` / `EnergyDeposit.Txt` from the unmodified code exist under `.scratch/tests/2026-09-28__multiple-dna-chemistry/baseline/` before any source edit in this ticket.
- [ ] `PureWaterReactions::BuildPureWaterAcidBase()` returns the 11 entries (`H`, `e_aq`, `O2m`, `HO2°`, `HO2m`, `Om`, `O3m`, `H2O2`, `°OH`, `OHm`, `H3Op`), in this order, with the exact rates, products and reaction types of the old hard-coded list.
- [ ] `DnaChemistryList::RegisterAcidBaseScavengerProcesses` takes an `AcidBaseList` and builds the same `ScavengerReactionAccess` processes from it; `ConstructProcess` passes `PureWaterReactions::BuildPureWaterAcidBase()`.
- [ ] Sorted reaction dump after the change equals the sorted baseline dump.
- [ ] `PhysicsInteractions.csv` and `EnergyDeposit.Txt` equal the baseline (same seed, physics stage only).
- [ ] The run succeeds (exit 0, `[RunAccumulatorMessenger] dumped and reset` and `The simulation took` present, no `EEEE`/`FatalException`) and species yields look plausible.

**Files to Touch:**
- `header/PureWaterReactions.hh`
- `src/PureWaterReactions.cc`
- `header/DnaChemistryList.hh`
- `src/DnaChemistryList.cc`

**Verification Step:**

Run (from the repo root, after the edits; the baseline was captured in step 1 of the Notes):
```bash
cd build
./sim.exe gps_dump.in --dir C:/DEV/GEANT4/SIM/dnachem-min/.scratch/tests/2026-09-28__multiple-dna-chemistry/after02 > after02.log 2>&1
S=C:/DEV/GEANT4/SIM/dnachem-min/.scratch/tests/2026-09-28__multiple-dna-chemistry
diff <(sort $S/baseline/ReactionTable.txt) <(sort $S/after02/ReactionTable.txt) && echo DUMP_SAME
diff $S/baseline/PhysicsInteractions.csv $S/after02/PhysicsInteractions.csv && echo PHYS_SAME
diff $S/baseline/EnergyDeposit.Txt $S/after02/EnergyDeposit.Txt && echo EDEP_SAME
grep -c "dumped and reset\|The simulation took" after02.log
grep -c "EEEE\|FatalException" after02.log
```

Expected:
`DUMP_SAME`, `PHYS_SAME`, `EDEP_SAME`, then `2` (both success markers) and `0` failure markers. Run the simulation with `run_in_background` and read the log, per `.claude/geant4-instructions.md` section 3.

**Notes:**

Step 1, before touching any source. Build the run binary as it is on `main` plus ticket 01 (which changes nothing at runtime), then capture the baseline. Write the scratch macro `build/macro/gps_dump.in` (untracked; `sim` prepends `macro/` itself, so pass the file name only):
```
/run/verbose 0
/tracking/verbose 0
/dnaLogger/verbose Info
/process/dna/e-SolvationSubType Ritchie1994
/process/chem/TimeStepModel SBS
/chem/reaction/dump ReactionTable.txt
/run/initialize
/gun/particle e-
/gun/energy 10 keV
/run/beamOn 2
/run/dumpDataAndReset
```
Build and run, with the MSVC wrapper from `.claude/geant4-instructions.md` section 1:
```powershell
$vs = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
cmd /c "`"$vs\VC\Auxiliary\Build\vcvars64.bat`" && cmake --build build --config RelWithDebInfo --target sim"
```
```bash
cd build
./sim.exe gps_dump.in --dir C:/DEV/GEANT4/SIM/dnachem-min/.scratch/tests/2026-09-28__multiple-dna-chemistry/baseline > baseline.log 2>&1
```
Check that `baseline/ReactionTable.txt` is non-empty and contains both the bimolecular reactions and the acid-base ones (for example a line naming `H3Op(B)`). If the acid-base lines are missing, stop: the dump cannot prove the refactor. The baseline files are git-ignored; do not commit them.

Step 2, `header/PureWaterReactions.hh`: add `#include "ChemistryTypes.hh"` and, inside the namespace, after `BuildPureWaterReactions`:
```cpp
  /// The pH-driven acid-base buffer equilibria against the bulk H3Op(B) /
  /// OHm(B) / H2O pseudo-species (UHDR: ChemPureWaterBuilder::
  /// WaterScavengerReaction), as plain data. Species are named as stored by
  /// G4ChemDissociationChannels_option1; the driver resolves them.
  ChemistryTypes::AcidBaseList BuildPureWaterAcidBase();
```
Update the file's header comment: it now supplies both the reaction table and the acid-base list, and needs `ChemistryTypes.hh` (copy both files to port it).

Step 3, `src/PureWaterReactions.cc`: add `#include <string>` if needed, and at the end of the file (`kOH` and `kHO2` already exist in the anonymous namespace):
```cpp
ChemistryTypes::AcidBaseList PureWaterReactions::BuildPureWaterAcidBase()
{
  const G4double M = 1e-3 * m3 / (mole * s);  // bimolecular unit (M^-1 s^-1)
  const G4double cW = 55.3;                   // bulk water molarity factor

  // Same values and order as the former hard-coded list in DnaChemistryList.
  return {
    {"H", {{"H2O", 6.32 / s, {"e_aq", "H3Op(B)"}, 0}, {"OHm(B)", 2.49e7 * M, {"e_aq"}, 0}}},
    {"e_aq",
     {{"H3Op(B)", 2.25e10 * M, {"H"}, 0}, {"H2O", 1.57e1 * cW / s, {"H", "OHm(B)"}, 0}}},
    {"O2m", {{"H3Op(B)", 4.78e10 * M, {kHO2}, 6}, {"H2O", 0.15 * cW / s, {kHO2, "OHm(B)"}, 0}}},
    {kHO2, {{"OHm(B)", 1.27e10 * M, {"O2m"}, 0}, {"H2O", 7.58e5 / s, {"H3Op(B)", "O2m"}, 6}}},
    {"HO2m",
     {{"H3Op(B)", 4.78e10 * M, {"H2O2"}, 0}, {"H2O", 1.36e6 * cW / s, {"H2O2", "OHm(B)"}, 7}}},
    {"Om", {{"H3Op(B)", 9.56e10 * M, {kOH}, 0}, {"H2O", 1.8e6 * cW / s, {kOH, "OHm(B)"}, 8}}},
    {"O3m", {{"H3Op(B)", 9.0e10 * M, {kOH, "O2"}, 0}, {"H2O", 2.66e3 / s, {"Om", "O2"}, 0}}},
    {"H2O2", {{"H2O", 7.86e-2 / s, {"HO2m", "H3Op(B)"}, 0}, {"OHm(B)", 1.27e10 * M, {"HO2m"}, 7}}},
    {kOH, {{"OHm(B)", 1.27e10 * M, {"Om"}, 8}, {"H2O", 0.060176635 / s, {"Om", "H3Op(B)"}, 0}}},
    {"OHm", {{"H3Op(B)", 1.13e11 * M, {}, 0}}},
    {"H3Op", {{"OHm(B)", 1.13e11 * M, {}, 0}}},
  };
}
```
Compare each line against the current `build(...)` calls in `DnaChemistryList.cc` before deleting them: rates, products and the reaction type (the last number; `0` means unset). If the compiler picks the wrong `std::vector` constructor for a single-element list, write the element types out.

Step 4, `header/DnaChemistryList.hh`: `#include "ChemistryTypes.hh"`; change the declaration to
```cpp
  void RegisterAcidBaseScavengerProcesses(const G4DNABoundingBox& boundary,
                                          const ChemistryTypes::AcidBaseList& list) const;
```
and rewrite its doc comment: registers one `G4DNAScavengerProcess` per entry of `list`; an empty list registers nothing.

Step 5, `src/DnaChemistryList.cc`: add `#include "ChemistryTypes.hh"`. Replace the whole body of `RegisterAcidBaseScavengerProcesses` (the fifteen `Conf(...)` lookups, the `Rx` struct, the `build` lambda and the eleven `build(...)` calls) with:
```cpp
void DnaChemistryList::RegisterAcidBaseScavengerProcesses(
  const G4DNABoundingBox& boundary, const ChemistryTypes::AcidBaseList& list) const
{
  auto* ph = G4PhysicsListHelper::GetPhysicsListHelper();
  const G4String caller = "RegisterAcidBaseScavengerProcesses";

  // Registers against mol->GetDefinition() rather than a caller-supplied
  // class pointer: several species here (OHm, H3Op) have no dedicated
  // factory class, and using GetDefinition() uniformly avoids the previous
  // mismatch where Om's process was attached to G4Oxygen::Definition() (the
  // separate, unused "Oxy" species) instead of Om's actual definition (a
  // private G4MoleculeDefinition("O", ...) created inside
  // G4ChemDissociationChannels_option1::ConstructMolecule()) -- silently
  // making those reactions unreachable.
  for (const auto& entry : list) {
    auto* mol = Conf(entry.molecule, caller);
    auto* process = new ScavengerReactionAccess("G4DNAScavengerProcess", boundary);
    for (const auto& r : entry.reactions) {
      auto* rd = new G4DNAMolecularReactionData(r.rate, mol, Conf(r.partner, caller));
      for (const auto& product : r.products) {
        rd->AddProduct(Conf(product, caller));
      }
      if (r.reactionType != 0) {
        rd->SetReactionType(r.reactionType);
      }
      process->SetReaction(mol, rd);
    }
    auto* def = const_cast<G4MoleculeDefinition*>(mol->GetDefinition());
    ph->RegisterProcess(process, def);
  }
}
```
In `ConstructProcess`, change the call to `RegisterAcidBaseScavengerProcesses(*chemWorld->GetChemistryBoundary(), PureWaterReactions::BuildPureWaterAcidBase());` (ticket 03 replaces this with the selected Chemistry). Remove `kOH`, `kHO2` and `<initializer_list>` from `DnaChemistryList.cc` if nothing else in the file uses them (grep first), to avoid unused-variable warnings. Update the file header comment so it points at the Chemistry data for the acid-base values.

Step 6: rebuild `sim`, run the verification block, then commit. Commit message: `refactor: move PureWater acid-base list into data`.
