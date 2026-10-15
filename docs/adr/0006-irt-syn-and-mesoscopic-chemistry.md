---
status: accepted
---

# Chemistry runs IRT_syn, then a mesoscopic stage; SBS is removed

> Superseded in part by [[0008-selectable-chemistry-modes]]: SBS is selectable again, and the mesoscopic stage can be switched off. The IRT_syn + mesoscopic model below remains the model of record.

Each event's chemical stage now has two parts, following the Geant4-DNA `UHDR` example. First, a particle-based stage (`G4DNAIndependentReactionTimeModel`, IRT_syn) runs up to a configurable hand-over time (default 5 ns). Then the compartment-based mesoscopic stage (`G4DNAEventScheduler`, Gillespie on a cell mesh; 6.25 nm cells requested, 15.26 nm on the default 1 mm box after the mesh cap below) runs up to the end time, which now defaults to 1 s. The goal is staged: long-time yields of single tracks now, and later dose-sized multi-track runs (stage 2), both with a consumable O2 pool. The pool stays per event (see [[0004-scavenger-reactions-per-chemistry]]), consumed in both parts and restored for each event. Multi-track depletion will come from putting several tracks in one event, not from carrying state across events.

**Considered options.**
- *Classic IRT*: rejected. It ignores `G4DNAScavengerProcess` and `G4DNAScavengerMaterial`, so bulk reactions would need non-diffusing fake partner molecules. `G4DNAIRT` builds those fakes as real tracks, and the molecule counter counts them as species. It also can't consume a scavenger.
- *Keeping SBS selectable*: rejected in favour of a single model. SBS results recorded before removal are kept as the validation reference.

**Consequences.**
- **Scope change.** The project's rules said "no voxelization" and "no dedicated UHDR models". They now read as follows: the geometry stays one homogeneous water box (no voxel geometry or G4Vox), but the chemistry may use Geant4's mesoscopic cell mesh. Pulse structure and multi-track dose effects remain future work.
- **Bulk reactions twice.** Each Chemistry's bulk-reaction list is registered both as `G4DNAScavengerProcess`, for the particle-based stage, and as reaction-table entries against the bulk species, which the mesoscopic Gillespie stage reads through `G4DNAScavengerMaterial`. The rates stay as they are. Reaction types 6, 7 and 8 are equilibria in the mesoscopic stage. Partially diffusion-controlled reactions are marked type 1 directly in each Chemistry.
- **Outputs.** The species output covers the particle-based stage, and a separate mesoscopic species output (log time grid, 10 per decade by default) covers the rest. Geant4 doesn't report which reaction fired in the mesoscopic stage, so the reaction output covers the particle-based stage only.
- **MT** is supported, with one mesoscopic scheduler per worker, and validated against Serial.
- **Mesh cap.** Geant4's mesh index arithmetic is 32-bit. `G4DNAMesh::ConvertIndex` computes `index.x * pixels`, which overflows at the first coarsening when the mesh starts above 65536 cells per side (fatal `G4DNAMesh013`, found by the spike). The initial cell count is therefore capped at 65536. On the default 1 mm box that gives 15.26 nm cells, not UHDR's 6.25 nm. We kept the 1 mm box (dilute single-track conditions out to 1 s) and accepted the coarser cell, instead of a smaller box, a mesh sub-domain or a patched Geant4. Validation measures the effect by comparing a 5 ns and a 20 ns hand-over.

**Decision and validation.** IRT_syn + mesoscopic is the model of record. The SBS difference is documented, not gated: the SBS dumps are a regression baseline, not ground truth. Full report: `docs/irt-syn-mesoscopic-validation.md`.
- G(1 us) vs SBS, 10 keV e-, 5 ns hand-over: water (N = 40) e_aq +25.0 %, OH +21.2 %, H2O2 +51.1 %, H2 -1.9 %; O2 21 % (N = 1500) e_aq +9.0 %, OH -1.7 %, H2O2 +36.9 %, H2 +30.9 %. IRT_syn alone (no hand-over) already differs from SBS with an identical reaction table (water +16.2 / +7.3 / +42.7 / +2.6 %), so the mesoscopic stage adds about 9 points on e_aq and 14 on OH in water; the cause of that surplus and of the H2O2/H2 difference is open.
- Serial vs `--threads 4` (5 blocks of 40 events, batch means): max difference 3.04 % against a gate of 3.32 % (3 x the measured relative error): pass.
- Hand-over 5 vs 20 ns (pre-fix build, N = 40): differences of 1.8 to 3.7 %, at the statistical noise.
- 1 s runs complete for both pairs (N = 40, 10 threads; water 470 s, O2 2135 s). Wall time at 1 us versus SBS: 0.27 (water) and 0.23 (O2).
- Two defects found by the validation were fixed: `e0a5b88` (radiolytic O2 tracks left alive at the hand-over hung IRT_syn; they are merged into the bulk pool; O2 run now completes) and `baf4b29` (`SpeciesMeso` late records stale because `G4DNAEventScheduler::RecordTime` runs only after reaction steps; recorded after every Gillespie step; O2 e_aq vs SBS +159 % -> +9 %).
