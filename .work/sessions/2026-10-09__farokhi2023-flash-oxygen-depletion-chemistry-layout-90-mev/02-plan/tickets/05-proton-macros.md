# Ticket 05: proton-macros

**Model:** haiku
**Effort:** low

**Acceptance Criteria:**
- [ ] 4 new macros exist in `macro/`: `beam_farokhi_proton_o2_0pct.in`, `beam_farokhi_proton_o2_3pct.in`, `beam_farokhi_proton_o2_7pct.in`, `beam_farokhi_proton_o2_21pct.in`.
- [ ] Each follows the structure of `macro/beam_o2.in`/`macro/beam_tonneau2025.in`: `/run/verbose 0`, `/tracking/verbose 0`, `/dnaLogger/verbose Info`, `/process/dna/e-SolvationSubType Ritchie1994`, `/chem/select Farokhi2023`, `/chem/meso/enable false`, the file's O2 scavenger line, `/run/initialize`, `/scheduler/endTime 1 us`, `/gun/particle proton`, `/gun/energy 90 MeV`, `/run/beamOn 20`, `/run/dumpDataAndReset`.
- [ ] No `/chem/env/halfBox` override (uses the existing 500 µm default) and no `/primaryKiller/*` lines (protons are unaffected by `PrimaryKiller`, which only acts on the primary electron).
- [ ] The O2 scavenger line in each file uses the paper's molar concentration, not the project's `%` shortcut:
  - `beam_farokhi_proton_o2_0pct.in`: no `/chem/env/scavenger` line (anoxic)
  - `beam_farokhi_proton_o2_3pct.in`: `/chem/env/scavenger O2 31.1 uM`
  - `beam_farokhi_proton_o2_7pct.in`: `/chem/env/scavenger O2 72.7 uM`
  - `beam_farokhi_proton_o2_21pct.in`: `/chem/env/scavenger O2 218 uM`
- [ ] Each file has a short header comment naming the figure it targets (Farokhi et al. 2023, Fig. 2/Fig. 3) and its nominal O2 percentage, and noting the molar value is the paper's own Henry's-law conversion at 37 °C (not the project's `%` shortcut, which assumes a different constant).
- [ ] All 4 macros run to completion (see Verification Step) and produce non-empty `Species.Txt` with the expected particle/energy recorded in `Manifest.json`.

**Files to Touch:**
- `macro/beam_farokhi_proton_o2_0pct.in`
- `macro/beam_farokhi_proton_o2_3pct.in`
- `macro/beam_farokhi_proton_o2_7pct.in`
- `macro/beam_farokhi_proton_o2_21pct.in`

**Verification Step:**

Run (repeat for each of the 4 files, substituting the filename and an output subfolder):
```bash
cmake --build build --config RelWithDebInfo --target sim
cd build
./sim.exe beam_farokhi_proton_o2_21pct.in --dir ../.scratch/tests/2026-10-09__farokhi2023-flash-oxygen-depletion-chemistry-layout-90-mev/ticket05-proton-21pct > ticket05-proton-21pct.log 2>&1
```

Expected, for each of the 4 runs: exit 0; log contains `[RunAccumulatorMessenger] dumped and reset` and `The simulation took`; no `EEEE`/`FatalException`; `Manifest.json` records `"particle": "proton"`, `"energy_keV": 90000` (or equivalent per `RunManifest.cc`'s schema) and the file's O2 concentration; `Species.Txt` is non-empty with 19 distinct recorded times (per ticket 2) up to ~1 µs.

**Notes:**

The 90 MeV proton's LET (~0.57 keV/µm) is close to constant over the default 1 mm box, and the box comfortably contains the longest secondary-electron ranges (~0.3-0.4 mm) — this is a deliberate, documented approximation of the paper's nested 10x10x10 µm³ "sensitive volume" inside a 1 km box (see the session's grill notes), not an oversight. `/run/beamOn 20` is a starting point for a visible trend, not publication-grade statistics; raise it if the resulting G-value curves look too noisy when checked against ticket 7's analysis script.
