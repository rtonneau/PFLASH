# Ticket 06: manifest-macros-docs-scope

**Model:** sonnet

**Acceptance Criteria:**
- [ ] `RunManifest::Write` adds these top-level keys:
  - `chemistryModel`: `"IRT_syn+mesoscopic"`;
  - `handOverTime_ns`;
  - `voxelSize_nm`;
  - `mesoPixels`;
  - `mesoTimesPerDecade`;
  - `chemistryEndTime_ns`: already written by `RunManifest` (keep it, do not add a seconds variant).
  
  Each is one `Add` line.
- [ ] `macro/*.in` no longer contains `/process/chem/TimeStepModel SBS`. `beam_02.in` (the "SBS variant") is removed or repurposed, and every reference to it is updated.
- [ ] Project `CLAUDE.md`:
  - The scope paragraph is amended. Geometry stays one homogeneous water box (no voxel geometry or G4Vox). The chemistry runs IRT_syn, then the mesoscopic stage on Geant4's cell mesh. Pulse structure and multi-track dose effects are future work (stage 2). It cites ADR 0006.
  - The key-file entries for `PhysicsList.cc`, `DnaChemistryList.cc`, `TimeStepAction.cc`, `MesoSettings.cc`, `MesoMessenger.cc` and `MesoSpeciesCounter.cc` are updated or added.
  - The macro section lists the `/chem/meso/*` commands, the 1 s default end time (with the `/scheduler/endTime` override) and the end of SBS.
  - The output paragraph mentions `SpeciesMeso.*` and states that `Reactions.*` covers the particle stage only.
- [ ] The `sim-output` skill documents `SpeciesMeso.Txt/.csv` and the new manifest keys. The `DnaChemistryList.hh` and catalog header comments match the new design.
- [ ] A smoke run (10 keV × 2, end time 1 ms) writes a `Manifest.json` with all six keys holding the expected values.

**Files to Touch:**
- `src/scoring/RunManifest.cc`
- `macro/beam.in`, `macro/beam_o2.in`, `macro/beam_boscolo.in`, `macro/beam_02.in`
- `CLAUDE.md`, `.claude/skills/sim-output/SKILL.md`
- `header/chemistry/DnaChemistryList.hh`

**Verification Step:**

Run (PowerShell, MSVC env):
```bash
cmake --build build --target sim
cd build; ./sim 06-smoke.in --dir ../.scratch/tests/2026-10-02__irt-syn-mesoscopic/06 > ../.scratch/tests/2026-10-02__irt-syn-mesoscopic/06.log 2>&1
grep -E "chemistryModel|handOverTime_ns|voxelSize_nm|mesoPixels|mesoTimesPerDecade|chemistryEndTime_ns" ../.scratch/tests/2026-10-02__irt-syn-mesoscopic/06/Manifest.json
grep -rn "TimeStepModel SBS" ../macro || echo none
```

Expected:
The run exits 0. All six keys are present with the expected values (`"IRT_syn+mesoscopic"`, 5, 6.25, the pixel count, 10, 1e6 for a 1 ms end time). The macro grep prints `none`.

**Notes:**

`RunManifest.cc` is the only place listing manifest keys (ADR 0005). Keep the CLAUDE.md wording matched to the `CONTEXT.md` terms (**Particle-based stage**, **Mesoscopic stage**, **Hand-over time**). Don't run the long production macros here.
