"""Patch 1 reachability audit (ticket 01 of patch-1-2-demonstration-cost).

Question: can an IRT_syn dose-rate history put THREE OR MORE molecules of ONE
species at exactly the same point? That is the trigger of the octree defect that
patch 1 (coincident-neighbors.patch) fixes: G4OctreeFinder builds one octree per
species (key = molecule configuration ID), and a G4Octree node holds at most
max_per_node = 2 entries, so a third coincident entry of the same species can
never be separated (see geant4-issues.md, issue 1). Two coincident molecules of
one species, or any number of coincident molecules of DIFFERENT species, sit in
different trees or in a leaf and are handled by the stock kernel.

Usage:
  python coincidence_audit.py --catalog docs/explorer/catalog.json \\
         --runs .scratch/dose-rate-patches/necessity [--g4-source <geant4 source root>]

Four parts, all printed as ASCII tables:
  1. Geant4 source facts (grep of the facts the argument rests on; needs
     --g4-source or ${DEV_DIR}/GEANT4/geant4-v11.4.3-source; skipped if absent).
  2. Catalogue audit, per built-in Chemistry: every dissociation channel,
     bimolecular reaction and bulk (scavenger) reaction with >= 3 products of one
     species, then a "stacking" closure. Products of one event share one point
     (G4DNAMakeReaction: one position for all products; G4DNAScavengerProcess:
     products at the reactant's position), so a cluster is a multiset of species
     at one point. The closure applies, to every cluster, (a) a bulk reaction on
     one member (product at the same point) and (b) a reaction between two
     members at distance 0 (products at the same point) until nothing new
     appears, and reports the largest same-species count reached. Stock
     Geant4 never performs (b): its radiusNeighbors drops distance-zero
     partners, so (b) is shown only as an upper bound.
  3. Reaction logs of the necessity runs (<runs>/<config>/runs/<variant>/<run>/
     reactions.csv): products of each logged reaction counted per species. The
     logs carry no positions, so this finds same-species products of one reaction
     only; the stacking part is the closure of part 2.
  4. Initial state: the captured packets (<runs>/library/packet-*.txt, positions
     in nm, hex floats): molecules of one species (name, charge, status,
     occupancy) at bit-identical positions, per packet and across packets
     (packets are never reused in a history, ArrivalSchedule.cc, so across
     packets is the worst case).

Exit code: 0 when no same-species coincident triple is reachable, 1 when one
is, 2 on missing inputs.
"""
import argparse
import collections
import csv
import glob
import json
import os
import re
import sys

# Stock Geant4 facts: (file under the source root, regex, what it shows).
SOURCE_FACTS = [
    ("source/processes/electromagnetic/dna/management/include/G4Octree.hh",
     r"max_per_node\s*=\s*2", "a leaf holds at most 2 entries"),
    ("source/processes/electromagnetic/dna/management/include/G4Octree.hh",
     r"max_depth\s*=\s*100", "depth limit 100"),
    ("source/processes/electromagnetic/dna/management/include/G4Octree.icc",
     r"input_values\.size\(\)\s*<=\s*max_per_node", "node with > 2 entries is split (init_internal)"),
    ("source/processes/electromagnetic/dna/management/include/G4Octree.icc",
     r"\+\+current_depth", "depth counter shared across siblings"),
    ("source/processes/electromagnetic/dna/management/include/G4Octree.icc",
     r"boxes\[child\]\.contains\(p\)", "an entry is copied into every child that contains it"),
    ("source/processes/electromagnetic/dna/management/include/G4OctreeFinder.icc",
     r"fTreeMap\[key\]\.reset\(new Octree\(Mollist->begin\(\),Mollist->end\(\)", "one octree per list key (species)"),
    ("source/processes/electromagnetic/dna/management/src/G4ITTrackHolder.cc",
     r"int moleculeID = GetIT\(track\)->GetITSubType\(\);", "track lists are keyed by molecule ID"),
    ("source/processes/electromagnetic/dna/molecules/management/include/G4Molecule.hh",
     r"return GetMoleculeID\(\);", "ITSubType of a molecule = its configuration ID"),
    ("source/processes/electromagnetic/dna/utils/include/G4ChemicalMoleculeFinder.hh",
     r"G4ChemicalMoleculeFinder::Instance\(\)->BuildTreeMap\(trackMap\)", "tree built from the track holder lists"),
    ("source/processes/electromagnetic/dna/models/src/G4DNAIndependentReactionTimeStepper.cc",
     r"IsInteractionStep\(\)", "IRT_syn Prepare(): on every interaction step ..."),
    ("source/processes/electromagnetic/dna/models/src/G4DNAIndependentReactionTimeStepper.cc",
     r"BuildChemicalMoleculeFinder\(\)", "... the trees are rebuilt (products included)"),
    ("source/processes/electromagnetic/dna/models/src/G4DNAIndependentReactionTimeStepper.cc",
     r"FindNearestInRange\(trackA, key, fRCutOff, resultIndices\)", "partner search per partner species key"),
    ("source/processes/electromagnetic/dna/models/src/G4DNAMakeReaction.cc",
     r"BuildTrack\(trackA\.GetGlobalTime\(\), \(reactionSite \+ randP\)/2\)",
     "all products of a reaction at ONE position"),
    ("source/processes/electromagnetic/dna/processes/src/G4DNAScavengerProcess.cc",
     r"BuildTrack\(reactionTime, track\.GetPosition\(\)\)", "bulk-reaction products at the reactant's position"),
    ("source/processes/electromagnetic/dna/processes/src/G4DNAWaterDissociationDisplacer.cc",
     r"radialDistributionOfProducts", "dissociation products get random displacements"),
]

# Bulk partners owned by the pH buffer: always on. Anything else needs
# /chem/env/scavenger (e.g. O2) and is absent from the dose-rate study.
BASELINE_BULK_PARTNERS = {"H2O", "H2O(B)", "H3Op(B)", "OHm(B)", "H3Op", "OHm"}
CLUSTER_SIZE_CAP = 10


def ascii_key(text):
    """Species key: configuration string without non-ASCII characters (the
    radical dot appears as a mangled character in several files)."""
    return re.sub(r"[^\x20-\x7e]", "", text).strip()


def read_text(path):
    with open(path, encoding="utf-8", errors="replace") as handle:
        return handle.read()


# ---------------------------------------------------------------- part 1


def check_source_facts(root):
    print("== 1. Geant4 source facts (%s)" % root)
    ok = True
    for rel, pattern, meaning in SOURCE_FACTS:
        path = os.path.join(root, rel)
        found = None
        if os.path.exists(path):
            regex = re.compile(pattern)
            for number, line in enumerate(read_text(path).splitlines(), 1):
                if regex.search(line):
                    found = number
                    break
        status = "line %d" % found if found else "NOT FOUND"
        ok = ok and found is not None
        print("  %-9s %s:%s  %s" % ("ok" if found else "MISSING", os.path.basename(rel),
                                    status.split(" ")[-1] if found else "-", meaning))
    print("  source facts: %s" % ("all found" if ok else "SOME MISSING"))
    print()
    return ok


# ---------------------------------------------------------------- part 2


class Chemistry:
    def __init__(self, name, data):
        self.name = name
        self.config = {m["name"]: ascii_key(m["configuration"]) for m in data["molecules"]}
        self.diffusion = {m["name"]: m["D_m2s-1"] for m in data["molecules"]}
        self.channels = []
        for state in data["dissociation"]:
            for ch in state["channels"]:
                self.channels.append(("%s/%s" % (state["state"], ch["name"]), self.tracked(ch["products"])))
        self.reactions = []   # (label, reactants (tuple of keys), products)
        self.meso_only = 0
        for r in data["reactions"]:
            if r.get("mesoOnly"):
                self.meso_only += 1
                continue
            label = "%s + %s" % tuple(r["reactants"])
            self.reactions.append((label, tuple(self.key(n) for n in r["reactants"]), self.tracked(r["products"])))
        self.gated_species = set()  # scavengers that only exist with /chem/env/scavenger
        self.bulk = []        # (label, reactant key, products, baseline?)
        for b in data["bulk"]:
            label = "%s + %s(bulk)" % (b["reactant"], b["partner"])
            baseline = b["partner"] in BASELINE_BULK_PARTNERS
            if not baseline:
                self.gated_species.add(self.key(b["partner"]))
            self.bulk.append((label, self.key(b["reactant"]), self.tracked(b["products"]), baseline))

    def key(self, name):
        return self.config.get(name) or ascii_key(name)

    def tracked(self, names):
        """Products that become tracks: scavenger-material (bulk) names end in (B)."""
        return tuple(self.key(n) for n in names if not n.endswith("(B)"))


def max_multiplicity(species):
    return max(collections.Counter(species).values()) if species else 0


def closure(chem, seeds, with_gated_bulk, with_pairs):
    """Largest same-species count reachable from the seed clusters.

    Moves: bulk reaction on one member (baseline bulk, plus gated bulk when
    with_gated_bulk) and, when with_pairs, a reaction between two members at
    distance 0. Returns (max count, example path or None, capped?)."""
    pair_rules = collections.defaultdict(list)
    for label, reactants, products in chem.reactions:
        if len(reactants) == 2:
            pair_rules[frozenset(reactants) if reactants[0] != reactants[1] else (reactants[0],)].append(
                (label, reactants, products))
    bulk_rules = collections.defaultdict(list)
    for label, reactant, products, baseline in chem.bulk:
        if baseline or with_gated_bulk:
            if with_gated_bulk:
                # With a scavenger present, a product that is that scavenger joins the bulk
                # pool and is not a track (fpScavengerMaterial->find(product)).
                products = tuple(p for p in products if p not in chem.gated_species)
            bulk_rules[reactant].append((label, products))

    best = (0, None, ())
    capped = False
    seen = {}
    queue = collections.deque()
    for label, products in seeds:
        start = tuple(sorted(products))
        if start not in seen:
            seen[start] = (label,)
            queue.append(start)
    while queue:
        cluster = queue.popleft()
        path = seen[cluster]
        multiplicity = max_multiplicity(cluster)
        if multiplicity > best[0]:
            best = (multiplicity, path, cluster)
        successors = []
        counts = collections.Counter(cluster)
        for species in counts:
            for label, products in bulk_rules.get(species, []):
                rest = list(cluster)
                rest.remove(species)
                successors.append((label, tuple(sorted(rest + list(products)))))
        members = list(counts) if with_pairs else []
        for i, a in enumerate(members):
            for b in members[i:]:
                if a == b and counts[a] < 2:
                    continue
                rules = pair_rules.get((a,) if a == b else frozenset((a, b)), [])
                for label, reactants, products in rules:
                    rest = list(cluster)
                    rest.remove(a)
                    rest.remove(b)
                    successors.append((label, tuple(sorted(rest + list(products)))))
        for label, nxt in successors:
            if nxt in seen:
                continue
            if len(nxt) > CLUSTER_SIZE_CAP:
                capped = True
                continue
            seen[nxt] = path + (label,)
            queue.append(nxt)
    text = None
    if best[1] is not None:
        text = " ; ".join(best[1]) + "  =>  {" + ", ".join(best[2]) + "}"
    return best[0], text, capped


def reactive_coincident_pairs(chem, seeds):
    """Seed clusters holding two species that react with each other (secondary
    patch-1 defect: the stock finder drops distance-zero partners)."""
    pairs = set()
    for _, reactants, _ in chem.reactions:
        if len(reactants) == 2:
            pairs.add(frozenset(reactants) if reactants[0] != reactants[1] else (reactants[0],))
    found = collections.Counter()
    for label, products in seeds:
        counts = collections.Counter(products)
        members = list(counts)
        hit = False
        for i, a in enumerate(members):
            for b in members[i:]:
                if a == b and counts[a] < 2:
                    continue
                if ((a,) if a == b else frozenset((a, b))) in pairs:
                    hit = True
        if hit:
            found[label] += 1
    return sorted(found)


def audit_catalog(path):
    """Returns (events with >= 3 products of one species, Chemistries that reach 3
    by bulk stacking on the stock kernel)."""
    print("== 2. Catalogue audit (%s)" % path)
    data = json.loads(read_text(path))
    triples_total = 0
    stacked_total = 0
    header = ("Chemistry", "channels", "reactions", "bulk", "direct>=3", "pairs(x2)", "stack:bulk",
              "stack:bulk+gated", "stack:+pairs(bound)")
    rows = []
    notes = []
    for name, body in data["chemistries"].items():
        chem = Chemistry(name, body)
        direct_hits = []
        for label, products in chem.channels:
            if max_multiplicity(products) >= 3:
                direct_hits.append("channel " + label)
        for label, _, products in chem.reactions:
            if max_multiplicity(products) >= 3:
                direct_hits.append("reaction " + label)
        for label, _, products, _ in chem.bulk:
            if max_multiplicity(products) >= 3:
                direct_hits.append("bulk " + label)
        pairs = [label for label, _, products in chem.reactions if max_multiplicity(products) == 2]
        pairs += ["channel " + label for label, products in chem.channels if max_multiplicity(products) == 2]
        seeds = [("channel " + label, products) for label, products in chem.channels]
        seeds += [("reaction " + label, products) for label, _, products in chem.reactions]
        seeds += [("bulk " + label, products) for label, _, products, _ in chem.bulk]
        seeds = [s for s in seeds if s[1]]
        bulk_max, bulk_path, bulk_cap = closure(chem, seeds, with_gated_bulk=False, with_pairs=False)
        gated_max, gated_path, gated_cap = closure(chem, seeds, with_gated_bulk=True, with_pairs=False)
        bound_max, bound_path, bound_cap = closure(chem, seeds, with_gated_bulk=True, with_pairs=True)
        triples_total += len(direct_hits)
        if bulk_max >= 3:
            stacked_total += 1

        def cell(value, capped):
            return str(value) + ("+" if capped else "")

        rows.append((name, str(len(chem.channels)), str(len(chem.reactions)), str(len(chem.bulk)),
                     str(len(direct_hits)), str(len(pairs)), cell(bulk_max, bulk_cap), cell(gated_max, gated_cap),
                     cell(bound_max, bound_cap)))
        for hit in direct_hits:
            notes.append("  %s: direct >= 3: %s" % (name, hit))
        notes.append("  %s: events with 2 products of one species: %s" %
                     (name, ", ".join(sorted(set(pairs))) or "none"))
        if bulk_max >= 3:
            notes.append("  %s: bulk stacking (stock-feasible) reaches %d: %s" % (name, bulk_max, bulk_path))
        elif gated_max >= 3:
            notes.append("  %s: bulk stacking needs a gated (scavenger) reaction: %s" % (name, gated_path))
        if bound_max >= 3:
            notes.append("  %s: upper bound with distance-zero pair reactions reaches %d: %s" %
                         (name, bound_max, bound_path))
        zero_d = sorted(n for n, d in chem.diffusion.items()
                        if d == 0 and not n.endswith("(B)") and n not in ("None", "NoneM"))
        react_species = set()
        for _, reactants, _ in chem.reactions:
            react_species.update(reactants)
        zero_d_live = [n for n in zero_d if chem.config[n] in react_species]
        notes.append("  %s: tracked species with D = 0 in a reaction: %s" % (name, ", ".join(zero_d_live) or "none"))
        coincident = reactive_coincident_pairs(chem, seeds)
        notes.append("  %s: events whose products include a mutually reactive pair (secondary defect): %d" %
                     (name, len(coincident)))
        if chem.meso_only:
            notes.append("  %s: %d mesoscopic-only reactions skipped (no positions)" % (name, chem.meso_only))
    widths = [max(len(header[i]), max(len(r[i]) for r in rows)) for i in range(len(header))]
    print("  " + "  ".join(h.ljust(w) for h, w in zip(header, widths)))
    for r in rows:
        print("  " + "  ".join(c.ljust(w) for c, w in zip(r, widths)))
    print("  direct>=3 = events (channels, reactions, bulk reactions) with >= 3 products of one species;")
    print("  pairs(x2) = events with exactly 2; stack:* = largest same-species count at one point after the")
    print("  closure of the module header: bulk = bulk reactions on a member (baseline pH-buffer ones only),")
    print("  bulk+gated = also scavenger-gated bulk (O2), +pairs = also distance-zero pair reactions (never")
    print("  done by stock Geant4: upper bound only); '+' = cluster size cap reached")
    for line in notes:
        print(line)
    print()
    return triples_total, stacked_total


# ---------------------------------------------------------------- part 3


def scan_logs(runs):
    print("== 3. Reaction logs (%s)" % runs)
    files = sorted(glob.glob(os.path.join(runs, "*", "runs", "*", "*", "reactions.csv")))
    if not files:
        print("  no reactions.csv found")
        print()
        return None
    per_group = collections.OrderedDict()
    for path in files:
        parts = os.path.normpath(path).split(os.sep)
        group = (parts[-5], parts[-3])  # config, variant
        entry = per_group.setdefault(group, {"histories": 0, "reactions": 0, "pairs": 0, "triples": 0,
                                             "pairKinds": collections.Counter()})
        entry["histories"] += 1
        with open(path, newline="", encoding="utf-8", errors="replace") as handle:
            for row in csv.DictReader(handle):
                entry["reactions"] += 1
                products = [ascii_key(p) for p in row["products"].split(" + ") if p.strip()]
                m = max_multiplicity(products)
                if m >= 2:
                    entry["pairs"] += 1
                    entry["pairKinds"]["%s + %s -> %s" % (ascii_key(row["reactantA"]), ascii_key(row["reactantB"]),
                                                          " + ".join(sorted(products)))] += 1
                if m >= 3:
                    entry["triples"] += 1
    header = ("config", "variant", "histories", "reactions", "same-species pairs", "same-species triples")
    rows = [(c, v, str(e["histories"]), str(e["reactions"]), str(e["pairs"]), str(e["triples"]))
            for (c, v), e in per_group.items()]
    widths = [max(len(header[i]), max(len(r[i]) for r in rows)) for i in range(len(header))]
    print("  " + "  ".join(h.ljust(w) for h, w in zip(header, widths)))
    for r in rows:
        print("  " + "  ".join(c.ljust(w) for c, w in zip(r, widths)))
    kinds = collections.Counter()
    for e in per_group.values():
        kinds.update(e["pairKinds"])
    for kind, count in kinds.most_common():
        print("  pair kind (all groups): %d x %s" % (count, kind))
    triples = sum(e["triples"] for e in per_group.values())
    print("  same-species coincident triples in the logs: %d" % triples)
    print()
    return triples


# ---------------------------------------------------------------- part 4


def read_packet(path):
    molecules = []
    for line in read_text(path).splitlines():
        if not line.startswith("molecule "):
            continue
        tokens = line.split()
        name, charge, status, count = tokens[1], int(tokens[2]), int(tokens[3]), int(tokens[4])
        occupancy = tuple(int(t) for t in tokens[5:5 + count])
        position = tuple(float.fromhex(t) for t in tokens[5 + count:8 + count])
        molecules.append(((name, charge, status, occupancy), position))
    return molecules


def scan_packets(library):
    print("== 4. Captured packets (%s)" % library)
    files = sorted(glob.glob(os.path.join(library, "packet-*.txt")))
    if not files:
        print("  no packets found")
        print()
        return None
    worst_in_packet = 0
    union = collections.Counter()
    total = 0
    for path in files:
        local = collections.Counter()
        for species, position in read_packet(path):
            local[(species, position)] += 1
            union[(species, position)] += 1
            total += 1
        worst_in_packet = max(worst_in_packet, max(local.values()) if local else 0)
    worst_union = max(union.values()) if union else 0
    print("  packets: %d, molecules: %d" % (len(files), total))
    print("  largest same-species count at one bit-identical position: %d within a packet, %d across all packets"
          % (worst_in_packet, worst_union))
    print()
    return worst_union


# ---------------------------------------------------------------- main


def default_source():
    dev = os.environ.get("DEV_DIR")
    if dev:
        path = os.path.join(dev, "GEANT4", "geant4-v11.4.3-source")
        if os.path.isdir(path):
            return path
    return None


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--catalog", required=True, help="docs/explorer/catalog.json")
    parser.add_argument("--runs", required=True, help="necessity run root (holds <config>/runs/... and library/)")
    parser.add_argument("--library", help="packet library (default <runs>/library)")
    parser.add_argument("--g4-source", help="Geant4 11.4.3 source root (default ${DEV_DIR}/GEANT4/geant4-v11.4.3-source)")
    args = parser.parse_args()
    if not os.path.exists(args.catalog) or not os.path.isdir(args.runs):
        print("missing input: --catalog or --runs", file=sys.stderr)
        return 2

    source = args.g4_source or default_source()
    if source and os.path.isdir(source):
        check_source_facts(source)
    else:
        print("== 1. Geant4 source facts: skipped (no --g4-source and no DEV_DIR source tree)\n")

    direct, stacked = audit_catalog(args.catalog)
    log_triples = scan_logs(args.runs)
    packet_worst = scan_packets(args.library or os.path.join(args.runs, "library"))

    reachable = direct > 0 or stacked > 0 or (log_triples or 0) > 0 or (packet_worst or 0) >= 3
    print("VERDICT: patch 1 trigger (>= 3 same-species molecules at one point) is %s: "
          "%d catalogue events with >= 3 products of one species, %d Chemistries reach 3 by stacking, "
          "%d triples in the reaction logs, largest same-species coincident count in the packets %s."
          % ("REACHABLE" if reachable else "UNREACHABLE in IRT_syn dose-rate runs", direct, stacked,
             log_triples if log_triples is not None else "n/a",
             packet_worst if packet_worst is not None else "n/a"))
    return 1 if reachable else 0


if __name__ == "__main__":
    sys.exit(main())
