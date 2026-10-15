# dnachem-min

Minimal Geant4-DNA water-radiolysis chemistry. This glossary tracks the chemistry
vocabulary used in `DnaChemistryList`/`DnaChemistryWorld` and related design
discussions; it is not a spec or implementation log.

## Language

**Chemistry**:
A named, selectable definition of the reaction network (including its bulk
reactions: acid-base buffer and scavenger rates), chosen once per process before initialization. Names are PascalCase
and matched case-insensitively (`PureWater`, `BoscoloChem`, `Tonneau2025`, ...). All Chemistries
share the same dissociation channels and a common base molecule set; a Chemistry may
add **Extra molecules** of its own. `PureWater` is the default.
A Chemistry reproduces the network of a given paper or library; it is a variant of
the reaction content only, not of the time-step model, physics, or scoring.
_Avoid_: "chemistry model" (collides with the **Particle-based stage** /
**Mesoscopic stage** models, e.g. IRT_syn);
"chemistry list" (that is the Geant4 class `G4VUserChemistryList`, the driver).

**Pure-water chemistry** (the `PureWater` Chemistry, baseline chemistry):
The default Chemistry's reaction network: the diffusion-controlled radical reactions
between tracked molecules (e_aq, H, °OH, H2, H2O2, O2, O2⁻, HO2°, HO2⁻, O⁻, O3⁻...),
plus the pH-dependent acid-base buffer equilibria against the bulk `H3Op(B)`/`OHm(B)`
species. Active regardless of whether dissolved O2 is present — the network can
produce O2 purely from water radiolysis (e.g. H2O2 + °OH → HO2° → [pH equilibrium]
→ O2⁻, then O2⁻ + °OH → O2).
_Avoid_: "water chemistry" alone (ambiguous with the O2-scavenger addition below).

**Extra molecule**:
A molecule that exists only while a given **Chemistry** is selected, because its
reaction network needs a species the common base set lacks (e.g. HO3 under
`Tonneau2025`). It is a tracked molecule like any other (diffusion, species and
mesoscopic output) and is absent from every other Chemistry's output.
_Avoid_: "custom species" and "scavenger" (a **Scavenger** is a bulk background, not a
tracked molecule).

**Tonneau2025** (Chemistry):
The Chemistry reproducing the 73-reaction homogeneous pure-water network of Table 2 of
Tonneau et al., Phys. Med. Biol. 70 (2025) 235021, including its acid-base block and
the dissolved-O2 reactions. The paper solves that network as a homogeneous ODE from
100 ns; here the same reactions act on tracked molecules from the pre-chemical stage,
so the network is exercised at the earlier times the paper does not reach.
_Avoid_: "homogeneous chemistry" for the Chemistry itself (that is the paper's phase,
not a selectable name).

**Scavenger**:
An *exogenous* dissolved species present in the sample as a bulk component (e.g.
atmospheric O2, set via `/chem/env/scavenger O2 21 %`), acting as an additional
reactant/sink for tracked molecules. Its concentration is part of the environment
(shared by all Chemistries); the reactions it takes part in, and their rates, are
**Bulk reactions** of the selected Chemistry. A concentration of 0 means the
scavenger is absent. Consumed as it reacts, in both the **Particle-based stage** and
the **Mesoscopic stage**; restored to its initial concentration at the start of each
event's chemistry (the pool belongs to one event, never shared across events). Not
required for pure-water chemistry to
function or to produce O2 — see [[0001-baseline-acid-base-buffer]].
_Avoid_: using "scavenger" for the pH acid-base buffer network itself — that is
baseline pure-water chemistry, not a scavenger.

**Bulk species** (bulk component):
A homogeneous background concentration rather than an individually tracked,
diffusing molecule: the pH-owned pseudo-species (name suffix `(B)`, e.g. `H3Op(B)`,
`OHm(B)`), bulk water, and any **Scavenger** (which, unlike the pH pool, is keyed on
the ordinary species name, e.g. `O2` — the same species can exist both as tracked
radiolytic molecules and as a bulk background). Reactions against a bulk species
only fire as **Bulk reactions**. A bulk species is never an individual molecule and
never appears in the species output.

**Bulk reaction**:
A reaction of a tracked molecule with a bulk species, defined per molecule by the
selected Chemistry: the acid-base buffer equilibria (against `H3Op(B)`/`OHm(B)`/
water) and the scavenger reactions (e.g. e_aq + O2(bulk) → O2⁻). Distinct from the
bimolecular reactions between two tracked molecules. A Chemistry defines each bulk
reaction once; it applies in both stages of the chemistry. Bulk reactions are not
counted in the reaction output.
_Avoid_: "acid-base reaction" as the umbrella term — acid-base is only one kind of
bulk reaction.

**Chemistry mode**:
How an event's chemistry is run, chosen in the macro before initialisation: **SBS**
(step-by-step, particle-based all the way to the end time), **IRT_syn** (particle-based
only, no hand-over) or **IRT_syn + mesoscopic** (particle-based stage, then the
**Mesoscopic stage**; the default). The mesoscopic stage is only available after
IRT_syn. Independent of the **Chemistry** (reaction content) and of the **Scavenger**,
except that SBS treats every reaction as fully diffusion-controlled while IRT_syn
treats the catalogue's Type II/IV pairs as partially diffusion-controlled.
_Avoid_: "model" alone (also names physics models), "time-step model" in user-facing text.

**Particle-based stage**:
The first part of an event's chemistry, from the end of the pre-chemical stage to
the **Hand-over time**: every radiolytic molecule is an individual particle with a
position, and reactions between two of them are sampled from their separation
(independent reaction times, synchronous variant: IRT_syn). Species and reaction
counts of this stage are the species output and the reaction output.
_Avoid_: "microscopic stage" (fine informally, but use one name); "IRT" alone (the
classic, non-synchronous IRT is not used). Under the SBS **Chemistry mode** the
whole chemistry is a single particle-based stage.

**Mesoscopic stage**:
The second part of an event's chemistry, from the **Hand-over time** to the end
time: molecules are no longer individuals but counts per small cubic cell of the
water box, reacting within a cell and hopping between neighbouring cells
(compartment-based reaction-diffusion). It reaches long times (up to seconds) that
the particle-based stage cannot afford. Its species counts are a separate output;
which reaction fired is not recorded.
_Avoid_: "voxel geometry" or "voxelization" for its cells — the geometry stays one
homogeneous water box; the cells exist only inside the chemistry.

**Spatial snapshot**:
The state of one event's **Mesoscopic stage** mesh at one record time: every cell
holding at least one molecule, with its centre, its side (which grows as the mesh
coarsens) and its molecule count per tracked species. Opt-in output, one per
record time of the mesoscopic log grid.
_Avoid_: "voxel dump", "concentration map" (counts are stored; concentration is
derived from count and cell side).

**Hand-over time**:
The moment of an event's chemistry at which the **Particle-based stage** stops and
the **Mesoscopic stage** takes over, carrying every remaining molecule into the
cells. Configurable; a few nanoseconds by default.

**Dump**:
One flush of everything accumulated since the previous flush (or program start) into
output files, then a reset — issued by `/run/dumpDataAndReset` or
`/run/dumpDataAndResetToDir`, or by the automatic exit-time flush. A dump is the unit
of "one simulation" for output purposes: it may cover several `/run/beamOn` calls.
_Avoid_: "run" or "simulation" for this unit — a Geant4 run is a single `/run/beamOn`.

**Manifest**:
The structured description of one **Dump**, written next to the data files it
describes: what was simulated (beam per run, Chemistry, environment), how (seed,
threads), the totals (events, energy deposit) and which files the dump produced.
Every dump has exactly one.
_Avoid_: "metadata" (already names `ReactionsMetadata.csv`, the reaction-id → label map).

**Pre-chemical file**:
The per-event record of what the physical stage handed to the chemical stage: every
ionised or excited water molecule and every solvated electron, with energy and
position. One file per event, including an empty one when the event made none. It
belongs to the **Dump** that covers its run and is listed in that dump's **Manifest**.
_Avoid_: "output_event file", "chemistry output" (the chemical stage's own results are
the species and reaction files).

**Primary track**:
The path of the one particle the gun fires per event (track 1), through the physical
stage only: its length, its kinetic energy at start and end, and how it ended
(stopped in the water, escaped the box, or killed by the primary killer).
_Avoid_: "primary particle" for the length (the particle is the thing, the track is its path).

**Secondary electron**:
Any electron of an event other than the primary, whatever its generation (the whole
delta-ray tree). A **First-generation secondary electron** is one whose parent is
the primary itself. Their summed track lengths and counts are recorded per event.
_Avoid_: "delta electron" (usually implies an energy threshold), "daughter" alone
(ambiguous between first generation and all generations).
