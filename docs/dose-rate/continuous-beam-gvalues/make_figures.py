import sys, csv, math
sys.path.insert(0, r"C:/Users/rtonneau/.claude/skills/ppt_figures")
import pptfig as pf
OUT = "docs/dose-rate/continuous-beam-gvalues"
D = "docs/dose-rate/continuous-beam-gvalues/"
def rd(p): return list(csv.DictReader(open(D+p, encoding="utf-8")))
def f(x): return float(x) if x not in ("", "nan") else math.nan
E, T = rd("gvalues_1ms.csv"), rd("gvalues_time.csv")
SP = ["e_aq^-1", "°OH^0", "H^0", "H_2^0", "H2O2^0", "H3O^1", "OH^-1"]
LAB = {"e_aq^-1": "e$_{aq}^-$", "°OH^0": "·OH", "H^0": "H", "H_2^0": "H$_2$", "H2O2^0": "H$_2$O$_2$", "H3O^1": "H$_3$O$^+$", "OH^-1": "OH$^-$"}
RATES = ["0.1", "1.0", "10.0", "100.0", "1000.0"]
pf.apply_style(); pf.register_series(SP + ["r" + r for r in RATES])
cond = "100 keV e-, continuous Poisson beam, 5 um reflecting water box, IRT_syn to 1 ms, pure water"
def meta(title, data): return {"title": title, "data": data, "conditions": cond, "source": "poisson-track-irt dose-rate study", "message": "[TODO]", "caption": "[TODO]", "status": "draft", "generated_by": "docs/dose-rate/continuous-beam-gvalues/make_figures.py"}
def sel(s): return sorted((r for r in E if r["species"] == s), key=lambda r: f(r["rate_Gy_s"]))
def g_rate(ax):
    ax.set_xscale("log"); ax.set_xlabel("dose rate (Gy/s)"); ax.set_ylabel("G at 1 ms (molecules / 100 eV)")
    for s in SP:
        r = sel(s); ax.errorbar([f(x["rate_Gy_s"]) for x in r], [f(x["G_per_100eV"]) for x in r], yerr=[f(x["SE"]) for x in r], fmt="o-", capsize=3, **pf.series(s, label=LAB[s]))
pf.save_single("g1ms_vs_rate", g_rate, meta=meta("G(1 ms) vs dose rate", D+"gvalues_1ms.csv"), legend=True, legend_kwargs={"ncol": 4})
def delta(ax):
    ax.set_xscale("log"); ax.set_xlabel("dose rate (Gy/s)"); ax.set_ylabel("G(1 ms) − G$_{0.1 Gy/s}$ (molecules / 100 eV)"); ax.axhline(0, color="grey", lw=1)
    for s in SP:
        r = [x for x in sel(s) if x["rate_Gy_s"] != "0.1"]
        ax.errorbar([f(x["rate_Gy_s"]) for x in r], [f(x["delta_vs_ref"]) for x in r], yerr=[f(x["delta_SE"]) for x in r], fmt="o-", capsize=3, **pf.series(s, label=LAB[s]))
pf.save_single("delta_vs_ref", delta, meta=meta("Change of G(1 ms) from the 0.1 Gy/s reference", D+"gvalues_1ms.csv"), legend=True, legend_kwargs={"ncol": 4})
for s, fid in [("e_aq^-1", "gt_eaq"), ("°OH^0", "gt_oh"), ("H_2^0", "gt_h2"), ("H2O2^0", "gt_h2o2")]:
    def gt(ax, s=s):
        ax.set_xscale("log"); ax.set_xlabel("time (ns)"); ax.set_ylabel(f"G({LAB[s]}) (molecules / 100 eV)")
        for rt in RATES:
            r = [x for x in T if x["species"] == s and f(x["rate_Gy_s"]) == float(rt) and not math.isnan(f(x["G_per_100eV"]))]
            t = [f(x["time_ns"]) for x in r]; g = [f(x["G_per_100eV"]) for x in r]; e = [f(x["SE"]) for x in r]
            st = pf.series("r" + rt, label=f"{float(rt):g} Gy/s")
            ax.plot(t, g, **st); ax.fill_between(t, [a-b for a, b in zip(g, e)], [a+b for a, b in zip(g, e)], color=st["color"], alpha=0.25, lw=0)
    pf.save_single(fid, gt, meta=meta(f"G({s})(t) per dose rate", D+"gvalues_time.csv"), legend=True, legend_kwargs={"ncol": 5})
for s in SP:
    print(s, [(x["rate_Gy_s"], round(f(x["G_per_100eV"]),3), round(f(x["SE"]),3), x["delta_vs_ref"][:6], x["delta_SE"][:5]) for x in sel(s)])
