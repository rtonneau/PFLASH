"""Reader of the snapshots.h5 a `history` run writes with `snapshots = true`.

Layout (format version 1, see src/SnapshotWriter.hh):
  /species                     species names, the table the ids below index
  /frames/frameNNNNNN          one group per frame, attribute time_ns
      position_nm  float32 [N, 3]   nm, box centre at 0
      species      uint8   [N]      index into /species

    import snapshots
    data = snapshots.read("snapshots.h5")
    for frame in data.frames:
        print(frame.time_ns, len(frame))
"""
from dataclasses import dataclass
from typing import List, Optional, Sequence

import numpy as np

try:
    import h5py
except ImportError as error:  # pragma: no cover
    raise SystemExit("snapshots.py needs h5py (pip install h5py)") from error


@dataclass
class Frame:
    time_ns: float
    positions_nm: np.ndarray  # float32 [N, 3]
    species: np.ndarray  # uint8 [N]

    def __len__(self) -> int:
        return int(self.species.shape[0])


@dataclass
class Snapshots:
    format_version: int
    species_names: List[str]
    frames: List[Frame]

    def times_ns(self) -> np.ndarray:
        return np.array([f.time_ns for f in self.frames])

    def species_id(self, name: str) -> int:
        return self.species_names.index(name)


def read(path: str, species: Optional[Sequence[str]] = None) -> Snapshots:
    """Read every frame. `species`: keep only the molecules of these names."""
    with h5py.File(path, "r") as file:
        version = int(file.attrs["formatVersion"])
        if version != 1:
            raise ValueError(f"{path}: snapshot format version {version}, this reader knows 1")
        names = [n.decode() if isinstance(n, bytes) else str(n) for n in file["species"][:]]
        keep = None
        if species is not None:
            missing = [s for s in species if s not in names]
            if missing:
                raise ValueError(f"{path}: no species {missing}; the table has {names}")
            keep = np.array([names.index(s) for s in species], dtype=np.uint8)
        frames = []
        group = file["frames"]
        for key in sorted(group):
            frame = group[key]
            positions = frame["position_nm"][:].reshape(-1, 3)
            ids = frame["species"][:]
            if keep is not None:
                mask = np.isin(ids, keep)
                positions, ids = positions[mask], ids[mask]
            frames.append(Frame(float(frame.attrs["time_ns"]), positions, ids))
    return Snapshots(version, names, frames)


if __name__ == "__main__":
    import sys

    if len(sys.argv) != 2:
        raise SystemExit("usage: snapshots.py <snapshots.h5>")
    snapshots = read(sys.argv[1])
    print(f"{len(snapshots.frames)} frames, {len(snapshots.species_names)} species: {snapshots.species_names}")
    for frame in snapshots.frames:
        print(f"{frame.time_ns:14.6g} ns  {len(frame):6d} molecules")
