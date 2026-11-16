"""3D movie of the molecules of a `history` run (snapshots.h5), coloured by species.

    python movie.py snapshots.h5 --out movie.gif
    python movie.py snapshots.h5 --out movie.mp4 --species "e_aq^-1" "OH^0" --max-points 3000

One animation frame per snapshot frame, with the box outline and the time. The
output is an mp4 when ffmpeg is available and the name ends in .mp4, a GIF
otherwise (an .mp4 request without ffmpeg falls back to the same name with .gif).
--box-half-side-nm sets the box outline (default: the extent of the positions).
"""
import argparse
import os
import shutil
import sys

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402
import numpy as np  # noqa: E402
from matplotlib import animation  # noqa: E402

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import snapshots as snapshots_reader  # noqa: E402


def time_label(time_ns: float) -> str:
    for unit, scale in (("s", 1e9), ("ms", 1e6), ("us", 1e3), ("ns", 1.0), ("ps", 1e-3)):
        if time_ns >= scale:
            return f"{time_ns / scale:.3g} {unit}"
    return f"{time_ns * 1e3:.3g} ps"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("snapshots", help="snapshots.h5 written by history")
    parser.add_argument("--out", default="movie.gif", help="output file (.mp4 or .gif)")
    parser.add_argument("--species", nargs="+", help="draw only these species (names as in the species table)")
    parser.add_argument("--max-points", type=int, default=5000, help="draw at most this many molecules per frame")
    parser.add_argument("--box-half-side-nm", type=float, help="half side of the box outline")
    parser.add_argument("--fps", type=float, default=4.0)
    parser.add_argument("--dpi", type=int, default=80)
    parser.add_argument("--seed", type=int, default=1, help="seed of the subsampling")
    parser.add_argument("--spin", type=float, default=0.0, help="camera azimuth change per frame (degrees)")
    args = parser.parse_args()
    if args.max_points < 1:
        parser.error("--max-points must be positive")

    data = snapshots_reader.read(args.snapshots, args.species)
    if not data.frames:
        parser.error("the file has no frame")
    names = data.species_names
    drawn = args.species if args.species else names
    colours = {n: plt.get_cmap("tab20")(i % 20) for i, n in enumerate(names)}

    everything = np.concatenate([f.positions_nm for f in data.frames if len(f)] or [np.zeros((1, 3))])
    half = args.box_half_side_nm or float(np.ceil(np.abs(everything).max() / 100.0) * 100.0) or 1.0
    rng = np.random.default_rng(args.seed)

    figure = plt.figure(figsize=(7, 6.5))
    axes = figure.add_subplot(111, projection="3d")

    def draw(index: int):
        frame = data.frames[index]
        axes.clear()
        corners = np.array([[x, y, z] for x in (-half, half) for y in (-half, half) for z in (-half, half)])
        for i in range(8):
            for j in range(i + 1, 8):
                if np.count_nonzero(corners[i] != corners[j]) == 1:
                    axes.plot(*zip(corners[i], corners[j]), color="0.5", linewidth=0.8)
        axes.set_xlim(-half, half)
        axes.set_ylim(-half, half)
        axes.set_zlim(-half, half)
        axes.set_box_aspect((1, 1, 1))
        axes.set_xlabel("x (nm)")
        axes.set_ylabel("y (nm)")
        axes.set_zlabel("z (nm)")
        if args.spin:
            axes.view_init(elev=20, azim=30 + args.spin * index)
        keep = np.arange(len(frame))
        if len(frame) > args.max_points:
            keep = np.sort(rng.choice(len(frame), args.max_points, replace=False))
        positions, ids = frame.positions_nm[keep], frame.species[keep]
        for name in drawn:
            mask = ids == names.index(name)
            if mask.any():
                axes.scatter(*positions[mask].T, s=6, color=colours[name], label=name, depthshade=False)
        axes.legend(loc="upper left", fontsize=7, markerscale=2, ncol=2)
        shown = f", {len(keep)} drawn" if len(keep) < len(frame) else ""
        axes.set_title(f"t = {time_label(frame.time_ns)}   ({len(frame)} molecules{shown})")

    out = args.out
    writer = None
    if out.lower().endswith(".mp4"):
        if shutil.which("ffmpeg"):
            writer = animation.FFMpegWriter(fps=args.fps)
        else:
            out = out[:-4] + ".gif"
            print("ffmpeg not found: writing a GIF", file=sys.stderr)
    if writer is None:
        if not out.lower().endswith(".gif"):
            out += ".gif"
        writer = animation.PillowWriter(fps=args.fps)
    movie = animation.FuncAnimation(figure, draw, frames=len(data.frames))
    movie.save(out, writer=writer, dpi=args.dpi)
    print(f"{out}: {len(data.frames)} frames, {os.path.getsize(out)} bytes")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
