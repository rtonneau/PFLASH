import sys, os, json, csv, collections
src, dst, nb = sys.argv[1], sys.argv[2], int(sys.argv[3])
os.makedirs(dst, exist_ok=True)
acc = collections.OrderedDict(); hdr = []
meso = collections.OrderedDict()
tot_e = 0.0; tot_n = 0; wall = 0.0; man0 = None
for b in range(nb):
    d = os.path.join(src, f"blk{b}")
    with open(os.path.join(d, "Species_nt_species.csv"), encoding="utf-8") as f:
        for l in f:
            if l.startswith("#"):
                if b == 0: hdr.append(l)
                continue
            p = l.rstrip("\n").split(",")
            k = (p[0], p[3], p[4])
            v = acc.get(k, [0, 0, 0.0, 0.0])
            v[0] += int(p[1]); v[1] += int(p[2]); v[2] += float(p[5]); v[3] += float(p[6]); acc[k] = v
    with open(os.path.join(d, "SpeciesMeso.csv"), encoding="utf-8") as f:
        rd = csv.reader(f); next(rd)
        for t, s, c in rd:
            meso[(t, s)] = meso.get((t, s), 0) + int(c)
    m = json.load(open(os.path.join(d, "Manifest.json"), encoding="utf-8"))
    if man0 is None: man0 = m
    tot_e += m["totalEnergyDeposit_eV"]; tot_n += m["totalEvents"]
    wall += sum(r["wallTime_s"] for r in m["runs"])
with open(os.path.join(dst, "Species_nt_species.csv"), "w", encoding="utf-8") as f:
    f.writelines(hdr)
    for (sid, name, t), v in acc.items():
        f.write(f"{sid},{v[0]},{v[1]},{name},{t},{v[2]:.10g},{v[3]:.10g}\n")
with open(os.path.join(dst, "SpeciesMeso.csv"), "w", encoding="utf-8") as f:
    f.write("time_ns,species,count\n")
    for (t, s), c in meso.items(): f.write(f"{t},{s},{c}\n")
man0["totalEnergyDeposit_eV"] = tot_e; man0["totalEvents"] = tot_n
man0["runs"] = [{"run": 0, "events": tot_n, "wallTime_s": wall}]
json.dump(man0, open(os.path.join(dst, "Manifest.json"), "w", encoding="utf-8"), indent=1)
print(dst, tot_n, tot_e, wall)
