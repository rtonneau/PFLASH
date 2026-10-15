# Ticket 06: electron-macros

**Model:** haiku
**Effort:** low

**Acceptance Criteria:**
- [ ] 4 new macros exist in `macro/`: `beam_farokhi_electron_o2_0pct.in`, `beam_farokhi_electron_o2_2p5pct.in`, `beam_farokhi_electron_o2_5pct.in`, `beam_farokhi_electron_o2_21pct.in`.
- [ ] Each follows the same structure as ticket 5's proton macros, with `/gun/particle e-`, `/gun/energy 500 keV`, and an added `/primaryKiller/eLossMin 5 keV` line (plus `/primaryKiller/verbose 0`, explicit, to keep output quiet but the control visible) placed after `/run/initialize` (matching the existing placement convention in `macro/beam_o2.in`).
- [ ] The O2 scavenger line in each file uses the paper's molar concentration:
  - `beam_farokhi_electron_o2_0pct.in`: no `/chem/env/scavenger` line (anoxic)
  - `beam_farokhi_electron_o2_2p5pct.in`: `/chem/env/scavenger O2 26.0 uM`
  - `beam_farokhi_electron_o2_5pct.in`: `/chem/env/scavenger O2 51.9 uM`
  - `beam_farokhi_electron_o2_21pct.in`: `/chem/env/scavenger O2 218 uM`
- [ ] `/run/beamOn 50` (electron track-segment runs are cheap per event).
- [ ] Each file has a short header comment naming the figure it targets (Farokhi et al. 2023, Fig. 5), its nominal O2 percentage, and a one-line note on why `/primaryKiller/eLossMin 5 keV` is set (1% of the 500 keV beam energy, keeping the simulated segment's LET close to the nominal 0.5 MeV value — the paper does not state its own numeric threshold).
- [ ] All 4 macros run to completion and show evidence (per ticket 4's validated mechanism) that the primary was truncated well before the full 500 keV was deposited.

**Files to Touch:**
- `macro/beam_farokhi_electron_o2_0pct.in`
- `macro/beam_farokhi_electron_o2_2p5pct.in`
- `macro/beam_farokhi_electron_o2_5pct.in`
- `macro/beam_farokhi_electron_o2_21pct.in`

**Verification Step:**

Run (repeat for each of the 4 files):
```bash
cmake --build build --config RelWithDebInfo --target sim
cd build
./sim.exe beam_farokhi_electron_o2_21pct.in --dir ../.scratch/tests/2026-10-09__farokhi2023-flash-oxygen-depletion-chemistry-layout-90-mev/ticket06-electron-21pct > ticket06-electron-21pct.log 2>&1
```

Expected, for each of the 4 runs: exit 0; log contains `[RunAccumulatorMessenger] dumped and reset` and `The simulation took`; no `EEEE`/`FatalException`; `Manifest.json` records `"particle": "e-"`, `"energy_keV": 500` and the file's O2 concentration, with its energy-deposit figure well under 500 keV x 50 events (confirming track-segment truncation per ticket 4); `Species.Txt` is non-empty with 19 distinct recorded times up to ~1 µs.

**Notes:**

This ticket is blocked on ticket 4's validation passing — do not wire `/primaryKiller/eLossMin` into these macros if ticket 4 found anomalous behavior; report back instead. The `2p5pct` filename spelling (not `2.5pct`) avoids a literal dot in the macro filename, consistent with the other 3 files in this ticket and ticket 5.
