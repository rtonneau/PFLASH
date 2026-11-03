# Plan: a real on/off switch for the primary-electron energy cuts

Date: 2026-11-03. Status: **to do (second part)**, not approved for execution yet.
Origin: Macro builder update (`docs/explorer`, plan `2026-11-03-explorer-macro-builder.md`).
The builder's step 5 now offers "Off: no energy cuts". The C++ has no switch, so the builder
emulates Off by writing `/primaryKiller/eLossMin 10 MeV` and `/primaryKiller/eLossMax 10 MeV`.
A primary electron is at most 1 MeV (the builder refuses more: `G4EmDNAPhysics_option2`
`emaxDNA = 1 MeV`), so it can never lose 10 MeV. This plan replaces the emulation with an explicit command.

## Current behaviour (verified in `src/scoring/PrimaryKiller.cc`)

- The constructor sets `fELossRange_Min = fELossRange_Max = DBL_MAX` and `fKineticE_Min = 0`.
  `DetectorConstruction::ConstructSDandField()` then sets the defaults to 500 keV and 1 MeV.
- `ProcessHits` first kills any track outside `fPhantomSize` (the box cut, all tracks). It then
  returns early unless the track is track 1 with PDG 11. Otherwise it accumulates `fELoss`, aborts
  the event when `fELoss > fELossRange_Max`, and kills the primary (setting `gPrimaryKilled`)
  when `fELoss >= fELossRange_Min || kineticEnergy <= fKineticE_Min`.
- `/primaryKiller/minKineticE` is created but has no `SetNewValue` branch. Its command
  `fpMinKineticE` is also **not deleted** in the destructor, unlike the other four.

## Changes

1. **Command.** In `PrimaryKiller`, add `G4UIcmdWithABool* fpEnergyCutsUI` →
   `/primaryKiller/energyCuts <bool>` (default `true`, guidance: "Apply the primary-electron
   energy-loss and kinetic-energy cuts; false keeps only the box cut"). Add a member
   `G4bool fEnergyCuts = true;`, a `SetNewValue` branch, a destructor `delete`, and a forward
   declaration in `header/scoring/PrimaryKiller.hh`. Optional: an inline
   `SetEnergyCutsEnabled(G4bool)` for C++ use, like the other setters.
2. **`ProcessHits`.** Leave the box cut unchanged. Extend the early return to
   `if (!fEnergyCuts || track->GetTrackID() != 1 || PDG != 11) return FALSE;`, so with the cuts off
   neither the abort nor the kill can fire, and `gPrimaryKilled` is set only by the box cut.
3. **Housekeeping found on the way (same commit or a separate one):** add the missing
   `delete fpMinKineticE;`. Either give `minKineticE` its handler
   (`fKineticE_Min = fpMinKineticE->GetNewDoubleValue(newValue)`) or remove the command. Then update the
   builder's "not offered" note and `CLAUDE.md` to match.
4. **Verify the `kineticEnergy <= fKineticE_Min` (= 0) branch before relying on the end reasons.**
   A primary that stops naturally may reach this branch and be reported as `killed` instead of
   `stopped` in `TrackLengths.*`, because `gPrimaryKilled` feeds `TrackingAction`. Check what
   post-step kinetic energy the stopping step reports. For example, `G4DNAOneStepThermalizationModel`
   proposes `fStopAndKill` plus a local deposit: does the post-step point carry 0 or the pre-step
   energy? Then check a `beam.in` run's `TrackLengths.csv` end-reason counts with
   `/primaryKiller/verbose 1`. If natural stops are mislabelled, set `gPrimaryKilled` only when the
   loss cut fires (or when `fKineticE_Min > 0` and the track is still alive). This matters more once
   the cuts can be off, because then every primary end should read `stopped` or `escaped`.
5. **Builder.** In `docs/explorer/macro-builder.js`, Off writes `/primaryKiller/energyCuts false`
   instead of the two 10 MeV lines; `KILLER_OFF` and its comment go. Update the `killerMode` tech
   text, `macro-builder.test.js` ('primary-electron energy cuts can be turned off'), and the README
   rule. Rebuild with `node docs/explorer/build.js`. The 'generated commands exist' test then also checks
   that `"energyCuts"` is in `PrimaryKiller.cc`.
6. **Manifest (optional).** Record the cut state (`primaryEnergyCuts: true|false` plus the two
   thresholds) in `RunManifest::Write`, so a dump says whether the primary was cut. This needs an
   accessor or a small process-wide setting. Decide whether it is worth it before doing it.
7. **Docs.** Update the `PrimaryKiller` paragraph in `CLAUDE.md` (new command; the `minKineticE`
   outcome) and the explorer `architecture.json` entry for `primary-killer`, if its summary lists
   the commands.

## Tests

- `PrimaryKiller` needs the tracking kernel, so it has no unit test. Use a smoke run instead (10 keV e-,
  `/run/beamOn 2`, `/scheduler/endTime 1 ms`) with `/primaryKiller/energyCuts false` and
  `/primaryKiller/verbose 1`: expect no `kill track at` line, and only `stopped`/`escaped` in
  `TrackLengths.csv`.
- A 1 MeV run with the cuts on (`eLossMin` 500 keV) versus off: the primary track length must
  increase with the cuts off.
- `node docs/explorer/macro-builder.test.js` and `ctest` (ExplorerCheck, MacroBuilderTest) in `build-ninja/`.

## Out of scope

Pulse structure, any change to the box cut (`/primaryKiller/setSize`), and cuts for protons or secondaries.
