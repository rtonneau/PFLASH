# Ticket 05: smoke-validation

**Status:** ✅ Done

## Local Test Result

Rerun in this session with the conda GEANT4_py311 interpreter (it has h5py):
- `check_meso_spatial.py .scratch/tests/.../t05` (Serial, 1 event, 10 keV, endTime 1 ms): `1 event(s), 55 snapshot(s), 16 species; 1-event full comparison` → `OK`. Summed counts match `SpeciesMeso.csv` at all 55 times. `O^0`, `O_3^-1` and `O_3^0` are all zero in the h5 and absent from the csv, which is consistent.
- `t05_mt` (`--threads 2`, 2 events): `2 event(s), 110 snapshot(s), 16 species; structure + monotonic cell size + box only` → `OK`.
- `t05_off` (switch off): no `.h5` and no `.pending_meso_spatial`.
- Size: 28.1 MB per event in Serial and 29 MB per event in MT, uncompressed (the vcpkg HDF5 has no deflate filter).

## Review Notes

Implemented by a sonnet subagent; reviewed in this session with no changes. All criteria hold: the 1-event comparison is exact (count tolerance 1e-9 relative; csv times matched within 1e-4 relative because the csv prints limited digits); cellSize is monotonic; positions are inside the `halfBox_um` box; MT and switch-off are checked. An h5-only species is a mismatch only if it has non-zero counts.

## Blockers / Challenges

- `sim.exe` takes the macro first and flags after; `--dir` before the macro is read as a macro name.
- With the switch on, an empty `.pending_meso_spatial/` folder stays in the dump dir after the move. `.pending_prechem/` behaves the same way.

## Commits

- 762cef3 test(meso): validate spatial snapshots against SpeciesMeso (ticket 05)

## Time Spent

3m (ticket-start.js to ticket-complete.js)
