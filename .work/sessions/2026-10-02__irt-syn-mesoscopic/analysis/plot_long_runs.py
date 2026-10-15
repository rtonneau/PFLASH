import sys
from pathlib import Path
REPO = Path(__file__).resolve().parents[4]  # .work/sessions/<session>/analysis/<this file>
sys.path.insert(0, str(REPO / "analysis"))
import sys, os
import compare_reference as c
import matplotlib; matplotlib.use("Agg")
import matplotlib.pyplot as plt
new, ref, out, title = sys.argv[1:5]
man, ser, wall, _ = c.load_dump(new)
rman, rser, _, _ = c.load_dump(ref)
names = {"e_aq":"e_aq^-1","OH":"°OH^0","H":"H^0","H2O2":"H2O2^0","H2":"H_2^0","HO2":"HO_2°^0"}
fig, axs = plt.subplots(2, 3, figsize=(12, 6.5), sharex=True)
tt = [5, 1e3, 1e6, 1e9]
print("N =", man["totalEvents"], "E =", man["totalEnergyDeposit_eV"])
for ax, (lab, n) in zip(axs.flat, names.items()):
    d = ser[n]; d = d[d.time_ns > 0]
    ax.plot(d.time_ns, d.G, label="IRT_syn+meso (1 s)")
    r = rser.get(n)
    if r is not None:
        r = r[r.time_ns > 0]; ax.plot(r.time_ns, r.G, "--", label="SBS (to 1 us)")
    ax.set_xscale("log"); ax.set_title(lab); ax.grid(alpha=.3)
    ax.axvline(5, color="gray", lw=.6); ax.axvline(1e3, color="gray", lw=.6, ls=":")
    vals = []
    for t in tt:
        x = d[d.time_ns <= t*(1+1e-5)]
        vals.append("%.4g" % x.iloc[-1].G if len(x) else "-")
    print(lab, "G at 5ns/1us/1ms/1s:", vals, " last t=%g" % d.time_ns.iloc[-1])
for ax in axs[1]: ax.set_xlabel("time [ns]")
for ax in axs[:,0]: ax.set_ylabel("G [/100 eV]")
axs[0,0].legend(fontsize=8)
fig.suptitle(title); fig.tight_layout(); fig.savefig(out, dpi=130)
