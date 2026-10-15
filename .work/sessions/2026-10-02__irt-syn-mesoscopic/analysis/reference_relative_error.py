import sys,csv,math
f=sys.argv[1]; tmax=1000.0
rows=[r for r in csv.reader(l for l in open(f,encoding='utf-8') if not l.startswith('#'))]
best={}
for r in rows:
    name=r[3]; t=float(r[4]); n=int(r[2]); g=float(r[5]); g2=float(r[6])
    if name not in best or abs(t-tmax)<abs(best[name][0]-tmax): best[name]=(t,n,g,g2)
for sp in ['e_aq^-1','°OH^0','H2O2^0','H_2^0']:
    for k,(t,n,g,g2) in best.items():
        if k.replace('\xb0','°')==sp or k==sp:
            m=g/n; var=(g2-g*g/n)/(n-1); se=math.sqrt(var/n)
            print(f"{k} t={t} N={n} G={m:.4f} sd_event={math.sqrt(var):.4f} SE={se:.4f} rel={100*se/m:.2f}% Nreq3%={math.ceil(var/(0.03*m)**2)}")
