import sys
from pathlib import Path
REPO = Path(__file__).resolve().parents[4]  # .work/sessions/<session>/analysis/<this file>
sys.path.insert(0, str(REPO / "analysis"))
import sys, math, os
import compare_reference as c
def stats(d, nb=5):
    vals = {k: [] for k in c.SPECIES}
    for b in range(nb):
        man, ser, wall, _ = c.load_dump(os.path.join(d, f"blk{b}"))
        for k, n in c.SPECIES.items():
            vals[k].append(c.g_at_1us(ser, n)[1])
    out = {}
    for k, v in vals.items():
        m = sum(v)/len(v); sd = math.sqrt(sum((x-m)**2 for x in v)/(len(v)-1))
        out[k] = (m, sd/math.sqrt(len(v)))
    return out
a = stats(sys.argv[1]); b = stats(sys.argv[2])
print("species  G_serial(+-SE)  G_mt4(+-SE)  rel.diff  relSE(diff)")
worst = 0
for k in c.SPECIES:
    (ma, sa), (mb, sb) = a[k], b[k]
    rel = (mb-ma)/ma; rse = math.sqrt(sa**2+sb**2)/ma
    worst = max(worst, rse)
    print(f"{k:5} {ma:.4f}+-{sa:.4f} {mb:.4f}+-{sb:.4f} {rel*100:+.2f}% {rse*100:.2f}% (SE rel. each {sa/ma*100:.2f}% / {sb/mb*100:.2f}%)")
print("max relSE(diff) = %.4f -> tol = 3x = %.4f" % (worst, 3*worst))
