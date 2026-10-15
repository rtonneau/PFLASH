import sys
from pathlib import Path
REPO = Path(__file__).resolve().parents[4]  # .work/sessions/<session>/analysis/<this file>
sys.path.insert(0, str(REPO / "analysis"))
import compare_reference as c
r=c.load_dump(str(REPO / 'validation/reference/sbs_water')); n=c.load_dump(sys.argv[1])
for sp in ['e_aq^-1','°OH^0','H2O2^0','H_2^0']:
    print(sp)
    for t in [1,2,5,6.3,10,20,50,100,200,500,1000]:
        def at(s):
            d=s[sp]; d=d[d.time_ns<=t*1.0001]; return d.iloc[-1].time_ns, d.iloc[-1].G
        print(' t=%g ref %.1f:%.4f new %.1f:%.4f'%((t,)+at(r[1])+at(n[1])))
