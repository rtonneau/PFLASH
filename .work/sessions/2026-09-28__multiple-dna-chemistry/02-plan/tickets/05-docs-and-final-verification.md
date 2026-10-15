# Ticket 05: docs-and-final-verification

**Acceptance Criteria:**
- [ ] `CLAUDE.md` Key Files and Macro sections describe `/chem/select`, `/chem/list`, the registry, the built-in Chemistries and `beam_boscolo.in`, and no longer say the acid-base network is unconditional without pointing at ADR 0002.
- [ ] The README has a short "Choosing a chemistry" section.
- [ ] File-header comments in `DnaChemistryList.hh/.cc` and `PureWaterReactions.hh` match the final design.
- [ ] `ctest` in `build-ninja` passes every test with none `Not Run`.
- [ ] Smoke runs succeed for the default and for `BoscoloChem`, and `macro/beam.in` is unchanged.
- [ ] `git status` shows no stray scratch or build output.

**Files to Touch:**
- `CLAUDE.md`
- `README.md`
- `header/DnaChemistryList.hh`
- `src/DnaChemistryList.cc`
- `header/PureWaterReactions.hh`

**Verification Step:**

Run:
```powershell
$vs = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
cmd /c "`"$vs\VC\Auxiliary\Build\vcvars64.bat`" && cmake --build build-ninja && cmake --build build --config RelWithDebInfo --target sim"
ctest --test-dir build-ninja --output-on-failure
```
```bash
cd build
S=C:/DEV/GEANT4/SIM/dnachem-min/.scratch/tests/2026-09-28__multiple-dna-chemistry
./sim.exe gps_dump.in --dir $S/final_default > final_default.log 2>&1
./sim.exe beam_boscolo.in --dir $S/final_boscolo > final_boscolo.log 2>&1
grep -c "dumped and reset\|The simulation took" final_default.log final_boscolo.log
grep -c "EEEE\|FatalException" final_default.log final_boscolo.log
diff <(sort $S/baseline/ReactionTable.txt) <(sort $S/final_default/ReactionTable.txt) && echo DEFAULT_SAME
git diff --stat -- macro/beam.in
git status --short
```

Expected:
CTest reports all tests `Passed` and none `Not Run`; `2` success markers and `0` failure markers per log; `DEFAULT_SAME`; no `beam.in` diff; `git status` lists only the intended documentation and source files. Run the simulations with `run_in_background`, read the heads of `Species.Txt` and `Reactions.Txt` in each output directory, and confirm the numbers look plausible.

**Notes:**

`CLAUDE.md` edits (match the existing paragraph style; keep it a project guide, not a changelog):
- Key Files: rewrite the `src/DnaChemistryList.cc` entry so the reaction table and the acid-base list come from the selected Chemistry (default `PureWater`), and add entries for `src/ChemistryRegistry.cc` (name to builder map and the once-per-process selection, kernel-free, covered by `test/ChemistryRegistryTest.cc`, portable together with `ChemistryTypes.hh`), `src/BuiltInChemistries.cc` (registers `PureWater` and `BoscoloChem`, called from the `DnaChemistryList` constructor), `src/ChemistrySelectMessenger.cc` (`/chem/select`, `/chem/list`), `src/PureWaterReactions.cc` (now also `BuildPureWaterAcidBase`) and `src/BoscoloChemReactions.cc` (work in progress, edit reactions there).
- Macro and Logging: add `/chem/select <name>` (PreInit, case-insensitive, default `PureWater`, unknown name or two different names is fatal) and `/chem/list`. Add `beam_boscolo.in` to the example macros.
- Adjust the sentence saying the acid-base network is baseline/unconditional: it holds for `PureWater`; a Chemistry may omit it (ADR 0002).
- Note how to add a Chemistry: copy `PureWaterReactions.{hh,cc}`, rename, register in `BuiltInChemistries.cc`.

`README.md`: read it first, then add a short "Choosing a chemistry" section that shows `/chem/select BoscoloChem` before `/run/initialize`, states the default, and points at `/chem/list`. Match the README's existing tone and heading level.

Source headers: `DnaChemistryList.hh` (the class comment still says the base chemistry is always the pure-water network), `DnaChemistryList.cc` (file header) and `PureWaterReactions.hh` should describe the final state: Chemistry chosen by name, `PureWater` the default and the reference implementation.

If `git status` shows tracked log noise such as `.claude/hooks/hook-posttooluse.log`, leave it out of the commit. Commit message: `docs: document named chemistries and /chem/select`.
