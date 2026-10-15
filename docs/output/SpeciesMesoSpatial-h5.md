# SpeciesMesoSpatial.h5

Where this file sits in the output directory: [README.md](README.md).

## Purpose

`SpeciesMesoSpatial.h5` holds the spatial state of the mesoscopic stage: for
each event and each record time, where the molecules are and how many of each
species sit in each occupied cell of Geant4's cell mesh. `SpeciesMeso.*` only
gives totals; this file lets you compute concentration maps.

The file exists only when a macro sets `/chem/meso/spatialOutput true` (before
`/run/initialize`; default false). Events append to a staging file
`<outdir>/.pending_meso_spatial/SpeciesMesoSpatial.h5`. Every dump moves it to
the dump target:

- `/run/dumpDataAndReset` writes `SpeciesMesoSpatial.h5` in the output
  directory;
- `/run/dumpDataAndReset <prefix>` writes `<prefix>SpeciesMesoSpatial.h5`;
- `/run/dumpDataAndResetToDir <subdir>` writes `<subdir>/SpeciesMesoSpatial.h5`;
- the safety-net flush at exit uses the prefix `EndOfRun_`.

There is one file per dump, covering all events since the previous dump. It is
listed in that dump's `Manifest.json` under `files`. The file is only written by
the application, never updated afterwards, and its name is the same in every
dump, so keep the prefix or the subfolder to tell dumps apart.

## Layout

```
/                                   root attributes: species, formatVersion, formatDoc, units
  run<R>/
    event<E>/
      snapshot<k>/                  attributes: time_ns, cellSize_nm
        position_nm                 dataset, N x 3, float64
        counts                      dataset, N x S, uint32
```

`<R>` is the run number, `<E>` the event number within the run (they are not
zero padded, so sort numerically), `<k>` the record index from 0.

## Attributes and datasets

| Name | Where | dtype | Shape | Unit | Meaning |
|---|---|---|---|---|---|
| `species` | root attribute | UTF-8 variable-length string | S | none | Column names of `counts`, in column order |
| `formatVersion` | root attribute | int32 | scalar | none | Format version, currently 2 |
| `formatDoc` | root attribute | UTF-8 string | scalar | none | Path of this document in the repository |
| `units` | root attribute | UTF-8 string | scalar | none | One-line summary of the units below |
| `time_ns` | `snapshot<k>` attribute | float64 | scalar | ns | Record time of the snapshot |
| `cellSize_nm` | `snapshot<k>` attribute | float64 | scalar | nm | Side of the cubic mesh cell at that time |
| `position_nm` | `snapshot<k>` dataset | float64 | N x 3 | nm | Cell centres (x, y, z) in the world frame |
| `counts` | `snapshot<k>` dataset | uint32 | N x S | molecules | Molecules per cell and species |

Row i of `counts` and row i of `position_nm` describe the same cell. Column j
of `counts` is the species `species[j]`.

## Sparse cells

Only cells holding at least one molecule (of the listed species) are written,
so N is the number of occupied cells and differs between snapshots. N can be 0
when no molecule is left. Cells that are absent are empty. A zero-row dataset
has shape (0, 3) or (0, S).

## Species columns

The columns are every configuration in Geant4's molecule table except:

- water in any state (`H2O` and the excited or ionised `H2O` states of the
  dissociation channels, which never reach the mesh);
- the `G4FakeMolecule` configuration named "None";
- bulk pseudo-species whose name ends in `(B)`.

Names are the Geant4 display names (for example `HO_2°^0`), stored as UTF-8
(h5py returns the attribute as `str`) and sorted. The list is fixed for a file;
the writer refuses to append an event with a different list. A species in the
list may never appear in the counts. The set depends on the selected Chemistry.

Caveat when mixing files from different Chemistries: an **Extra molecule** (for
example `HO3` under `Tonneau2025`) is a column only in files written with that
Chemistry selected. A `Tonneau2025` file has one more column than a `PureWater`
or `BoscoloChem` file, so `counts` arrays of different Chemistries cannot be
stacked or compared by column index. Always match columns by name through the
root attribute `species` (and the file's `chemistry` entry in the dump's
`Manifest.json`), never by position. A process uses one Chemistry only, so a
single file never mixes them.

## Snapshot times

Snapshots follow the mesoscopic log time grid: from the hand-over time (default
5 ns) to the end time, `/chem/meso/timesPerDecade` points per decade (default
10). `snapshot<k>` is the k-th record time, ascending in time, so k = 0 is the
earliest. Every record time gets a snapshot, including those after the last
reaction.

The mesh only changes at some steps, so several consecutive record times often
share one state. Their `position_nm` and `counts` are then the same HDF5
dataset reached through several names (hard links), stored once. Each snapshot
group still has its own `time_ns` and `cellSize_nm`. Reading through h5py works
as usual.

## Cell size

The mesh coarsens during the stage, so `cellSize_nm` grows with `time_ns`. Read
it per snapshot, never assume a constant. The first value is the requested
voxel size (`/chem/meso/voxelSize`, default 6.25 nm) adjusted to a power-of-two
pixel count, capped at 65536 pixels per side (about 15.26 nm on the default 1
mm box).

## Concentration

For a cell, in mol/L:

```
concentration = count / (N_A * (cellSize_nm * 1e-8)^3)
```

with N_A = 6.02214076e23 per mol. The factor `1e-8` converts nm to dm, so the
cube is the cell volume in litres.

## Compression

Datasets are gzip-compressed (level 4, chunks of up to 4096 rows) only when the
HDF5 build has the deflate filter. Otherwise they are stored uncompressed and
contiguous. Empty datasets are never compressed. Readers need no special step
either way.

## Example (h5py)

```python
import sys, h5py, numpy as np
N_A = 6.02214076e23

with h5py.File(sys.argv[1], "r") as f:
    species = list(f.attrs["species"])             # S column names (str)
    print("formatVersion", f.attrs["formatVersion"], "S =", len(species))
    events = [(r, e) for r in f for e in f[r]]     # e.g. ("run0", "event0")
    print("events:", events)

    run, event = events[0]
    ev = f[run][event]
    k = sorted(ev, key=lambda n: int(n[len("snapshot"):]))[-1]   # last snapshot
    snap = ev[k]
    time_ns = snap.attrs["time_ns"]
    size_nm = snap.attrs["cellSize_nm"]
    pos = snap["position_nm"][:]                   # (N, 3) float64, nm
    counts = snap["counts"][:]                     # (N, S) uint32

volume_L = (size_nm * 1e-8) ** 3
conc = counts / (N_A * volume_L)                   # mol/L, (N, S)
print(f"{k}: t = {time_ns:.3g} ns, cell = {size_nm:.4g} nm, N = {len(pos)}")
print("shapes:", pos.shape, counts.shape)
if len(pos):
    j = int(np.argmax(counts.sum(axis=0)))
    print(f"most abundant: {species[j]}, max {conc[:, j].max():.3g} mol/L")
```

## Version history

| Version | Change |
|---|---|
| 1 | Every cell of the mesh was written, empty ones included. |
| 2 | Only cells with at least one molecule are written (N can be 0); the root attribute `formatDoc` was added. |
