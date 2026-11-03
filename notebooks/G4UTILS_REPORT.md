# G4Utils work report from the executed vertical slice

## Evidence and scope

The [vertical-slice notebook](vertical_slice.ipynb) was executed top to bottom
in a fresh GEANT4 Jupyter kernel on 2026-11-03. Its two seeded, two-event
`PureWater`/`IRT_syn` demonstrations are documented in the
[notebook README](README.md): one has no mesoscopic stage and the other hands
over at 5 ns and writes mesoscopic aggregate and spatial output. Both manifests
report `geant4-11-04-patch-03` and 20,000 eV total deposited energy. The
notebook checked 12 and 15 nonempty files on disk, respectively; its raw-row
and normalization assertions passed. It parsed 2,152 pre-chemical rows and
found 999 solvated-electron rows with final positions in the IRT dump. Four
plots were reviewed. These are workflow checks on small runs, not convergence
or physics validation. The two runs have different seeds and are not one
continuous trajectory.

The G4Utils checkout used by that kernel is version 0.4.0 at
`a45629041f1d47b1e2d7b9b367aa0a95443a732f` (`${DEV_DIR}/PYTHON/G4Utils`).
At that revision, `g4utils.DnaChem` already provides `Dump`, `Simulation`,
`find_dumps`, `load_manifests`, `load_species`, `load_reactions`, reaction
metadata/table loading, manifest rendering, and the Geant4 `wcsv` ntuple
reader. `Dump.species()` retains the ntuple's `number`, `nEvent`, `sumG`, and
`sumG2` and adds `G = number / (totalEnergyDeposit_eV / 100)` and `time_s`.
`Dump.reactions()` joins reaction labels. `Dump.meso` and `Dump.has_meso` refer
to **`SpeciesMesoSpatial.h5`**, read by `SpeciesMesoSpatialFile`; neither
represents aggregate `SpeciesMeso.csv`. The implementation evidence is
`src/g4utils/DnaChem/{dump,dumps,simulation,loaders,manifest,ntuple}.py` in
that G4Utils revision, plus its DnaChem tests and README. None of the four
direct table readers below exists in that revision.

The simulation-format evidence is [output documentation](../docs/output/README.md),
[the output skill](../.claude/skills/sim-output/SKILL.md), and the current
writers `src/scoring/{PhysicsInteractionCounter,TrackLengthTable,ScoreSpecies,MesoSpeciesCounter}.cc`
and `src/actions/{SteppingAction,EventAction}.cc`. The notebook's direct
readers and assertions are visible in its cells. The configured Geant4 source
is 11.4.1 while the active install and generated data are 11.4.3. The notebook
establishes its observed fixed-width layout, but that mismatch prevents
claiming Geant4-source validation of the 11.4.3 pre-chemical field semantics.

## Priority 1: discover every dump in a results directory

**Need and current behavior.** A single output root may contain a results
index (`Manifest.json` with `kind: "resultsIndex"` and `dumps[]`), several
filename-prefixed dumps in that same directory, and nested subdirectory
dumps. A plain unprefixed dump collides with the index path: its manifest also
holds `dumps[]` but has no `kind`. Each index entry gives `folder`, `prefix`,
and a relative `manifest` path. The notebook had to point `Dump` and
`Simulation` at two separate unprefixed folders. In G4Utils `a456290`,
`find_dumps(root)` returns `[root]` as soon as `root/Manifest.json` exists;
otherwise it searches only direct children. `Dump(path)` requires exactly
`path/Manifest.json`; its data-file helper can prepend `manifest.prefix`, but
there is no way to select `pfx_Manifest.json`. `Simulation.subruns` is keyed by
folder name, so multiple prefixed dumps in one folder would collide. Passing
an index-only root can therefore be mistaken for a dump, and a prefixed or
nested dump is missed. This is a discovery issue; the existing prefix-aware
data-file helper is useful once the correct manifest is selected.

**Proposed interface and return data.** Add `discover_dumps(root) ->
list[DumpLocation]`, where each immutable location has a stable dump key,
directory, resolved manifest path, and literal prefix. Add
`Dump.from_manifest(manifest_path)` and let `Simulation(root)` enumerate those
locations without reducing them to a folder-name key. Keep the existing
single-dump path behavior for callers. Resolve index paths relative to the
index root, restrict them to that root, and use each selected manifest's
`prefix` and `files` for data files. Index order should be preserved. The
location carries no physical units or normalization; `events` and run
metadata in the index are discovery hints, while the selected dump manifest
remains authoritative for calculations.

**Missing data and acceptance checks.** An absent index may use the existing
single-dump/direct-child fallback. A malformed index, missing referenced
manifest, duplicate key, prefix mismatch, or path escaping the root should
raise a contextual error; a manifest-listed data file that is absent should
remain visibly absent rather than be inferred from the list. Fixtures must
cover index-only roots, the collided plain manifest, two prefixes in one
folder (including `EndOfRun_`), nested subdirectories, and missing/bad
references. Every selected dump must load its own species file and retain
distinct provenance. Compare discovery against a real multi-dump output,
which the two demonstration directories alone do not provide.

## Priority 2: reusable physical and track readers

### Physical interactions

**Need and evidence.** The notebook plots process firing totals after
reading `PhysicsInteractions.csv` directly with `pandas.read_csv`. The file
has `label,count`; the checked IRT dump begins with
`e-_G4DNAAttachment,17`. `SteppingAction.cc` records the process name when it
contains `G4DNA`, and `PhysicsInteractionCounter.cc` writes aggregate counts.
These are discrete process firings over a dump, not complete particle tracks
or a time or spatial history. G4Utils `Dump` has no reader for this CSV.

**Proposed interface and return data.** `Dump.physical_interactions() ->
pandas.DataFrame` returns `label` (string), `count` (integer), and dump
provenance columns. Preserve the raw process label. Counts are dimensionless
totals over all included events; optional `count_per_event` is `count /
manifest.totalEvents`, explicitly labelled as a mean firing count per event.
Never call this a G-value.

**Missing data and checks.** Missing CSV is a clear `FileNotFoundError` naming
the resolved prefixed path. A valid header-only CSV returns an empty frame
with fixed columns. Reject malformed headers, noninteger or negative counts,
and nonpositive `totalEvents` when event normalization is requested. Test raw
row equality, zero counts, prefix resolution, and an empty tally. Do not
silently substitute `PhysicsInteractions.Txt`.

### Track lengths

**Need and evidence.** The notebook reads `TrackLengths.csv` to compare
primary, first-generation secondary, and all-secondary electron path lengths
by `(run,event)`. Its first checked IRT row is run 0/event 0 with primary
length 3940.635938 nm and `primaryEnd=stopped`. `TrackLengthTable.cc` writes
one row per non-aborted event and the manifest's `trackLengths` object
summarizes those rows. The two-event demo has two rows, but that equality does
not hold when events are aborted. G4Utils has no track-length reader.

**Proposed interface and return data.** `Dump.track_lengths() ->
pandas.DataFrame` returns the CSV columns `run`, `event`,
`primaryLength_nm`, `primaryEkin0_keV`, `primaryEkinEnd_keV`, `primaryEnd`,
`secondaryFirstGen_nm`, `nSecondaryFirstGen`, `secondaryAll_nm`, and
`nSecondaryAll`, retaining `(run,event)` as the event identity within the
dump. Lengths are nm, kinetic energies keV, and secondary counts integers;
each row is an event, with no division by the manifest event total.

**Missing data and checks.** Missing CSV raises with the resolved path;
header-only output returns a typed empty frame. Validate required columns,
nonnegative lengths and counts, recognized end labels, and unique
`(run,event)` pairs. Compare rows with CSV and, when present, the manifest's
`trackLengths.events` and summary within output precision. A difference
between row count and `totalEvents` must be reported as possible aborted or
otherwise omitted events, not automatically treated as a parse failure.

## Priority 3: pre-chemical event records

**Need and evidence.** The notebook's visible fixed-width parser reads
`PreChemical_run<R>_event<E>.txt`, preserves run/event identity, counts `H2O`
modification records, and plots final `e_aq` positions when present. Its
observed widths are `[11,10,14,13,22,22,22,14,14,14]`. The checked first IRT
file begins with comment headers and water rows; 2,152 total records were
parsed across the two files. `EventAction.cc` stages one file per event and
moves it on dump. G4Utils has no reader or event-file inventory for these
records.

**Proposed interface and return data.** `Dump.prechemical_records() ->
pandas.DataFrame` returns one row per record, with `run`, `event`,
`parent_id`, `molecule`, raw `electron_modification`, `energy_eV`,
`parent_x_nm`, `parent_y_nm`, `parent_z_nm`, and nullable `x_nm`, `y_nm`,
`z_nm`. Discover files through the selected manifest's `files` first, using
the dump's literal prefix; parse the run/event numbers from each filename
with numeric sorting. The position columns are nm, energy is eV, and rows
are records, not event-normalized counts. Keep raw modification codes as
strings pending matched-version source verification; a notebook can label or
plot selected codes after stating its interpretation.

**Missing data and checks.** Missing files listed by a manifest should raise
with their names. If no pre-chemical files are listed or found, return an
empty typed frame only when the caller explicitly allows absent optional
output; otherwise explain how to regenerate it. An empty per-event file is
valid. Preserve null final coordinates; do not replace them with parent
coordinates. Reject incomplete required fields, bad numeric data, ambiguous
file names, and duplicate `(run,event)` files. Check fixed-width rows
against raw substrings, multiple runs with repeated event numbers, prefixed
files, empty events, water rows without final position, and `e_aq` rows with
and without final position. Confirm field semantics against the Geant4 source
matching the active binary before promoting modification-code labels to a
library contract.

## Priority 4: aggregate mesoscopic species

**Need and evidence.** The notebook reads `SpeciesMeso.csv` directly and
plots `count / manifest.totalEvents` against `time_ns`, with the hand-over
marked. The CSV is `time_ns,species,count`, with counts summed over events.
The companion `SpeciesMeso.Txt` says mean count per event; the checked first
CSV row is 97 `H2O2^0` molecules at 5 ns and the TXT value is 48.5 for two
events. `MesoSpeciesCounter.cc` confirms the CSV sum and TXT division.
G4Utils spatial HDF5 support does not read this aggregate CSV.

**Proposed interface and return data.** `Dump.meso_species() ->
pandas.DataFrame` returns `time_ns` (ns), `species` (raw string), `count`
(summed integer molecules), and `mean_count_per_event = count /
manifest.totalEvents` (molecules per event), with dump provenance. Retain
the raw species spelling rather than assuming that its identifiers equal the
particle-stage IDs. The hand-over and end times come from the manifest,
not from a guessed plot range. This is a count curve, not a G-value.

**Missing data and checks.** Add `Dump.has_meso_species`, true only when the
manifest enables mesoscopic output and the resolved CSV exists. If it is false,
`Dump.meso_species()` raises a `FileNotFoundError` that distinguishes disabled
mesoscopic output from a missing expected file. If the stage is enabled but end
time is at/before hand-over, a header-only file is valid and yields an empty
frame. Reject malformed rows and a nonpositive event denominator. Test the
97/2 = 48.5 normalization, prefixes, disabled output, header-only output, and
stage-on/missing-file inconsistency. Do not infer aggregate availability from
`Dump.has_meso`, which tests the spatial HDF5.

## Priority 5: name the two particle-stage G estimators

The existing `Dump.species().G` is the **aggregate-energy ratio**
`100 * number / totalEnergyDeposit_eV` in molecules per 100 eV. The writer
also stores `sumG = Σ G_i`, `sumG2 = Σ G_i²`, and `nEvent` for each
species/time row; `ScoreSpecies.cc` forms each event's `G_i = 100 *
count_i / deposited_energy_i_eV` before accumulating those sums. Thus
`sumG/nEvent` is **mean event G**, a different estimator when event deposits
differ. The notebook calculated and displayed both for a raw checked row.
Neither is the mesoscopic mean count per event. `totalEnergyDeposit_eV` is a
dump total and does not supply the individual event deposits, so the stored
aggregates cannot reconstruct event-level G/count correlations or a standard
error for the aggregate-energy ratio.

**Proposed interface and return data.** Add an explicit calculation such as
`Dump.particle_g_estimates() -> pandas.DataFrame`, retaining species/time and
raw moments and returning `aggregate_energy_G`, `mean_event_G`, and nullable
`mean_event_G_sem`, all in molecules per 100 eV. Document the legacy `G`
column as the aggregate-energy ratio; any rename needs a compatibility plan.
Require a finite positive total deposited energy for the ratio and positive
`nEvent` plus finite `sumG` for the mean; fail with a contextual error for
invalid required input instead of outputting infinity.

The sample SEM of mean event G is algebraically available from the two
moments only if **the same `nEvent` valid event values, including zeros, are
represented in both sums**, `nEvent >= 2`, all event deposits used to form G
were positive, and the moments are finite and consistent:

```text
mean_event_G_sem = sqrt(max(0, (sumG2 - sumG^2/nEvent)
                                / ((nEvent - 1) * nEvent)))
```

The current CSV alone does not prove event coverage or positive per-event
deposits, and `ScoreSpecies.cc` has no explicit zero-deposit guard at its
division. Therefore the library should leave `mean_event_G_sem` null by
default until the scorer invariant is verified or the output format records
the count of valid event G values. Even with that invariant, interpreting SEM
as uncertainty requires independent events; repeated or related seeds must
be identified separately. Test a synthetic unequal-deposit example where
the two estimators differ, equal-deposit agreement, `nEvent` 0/1, zero total
deposit, nonfinite/inconsistent moments, and eligible two-moment SEM.

## Library boundary and unresolved verification

G4Utils should own dump selection, format validation, units, provenance,
missing-file signaling, and the explicitly defined estimators. The notebooks
should own species selection, water-code interpretation until validated,
plot layout, axes, annotations, and comparisons across runs. Existing spatial
HDF5 access remains in `SpeciesMesoSpatialFile`; this report proposes no
replacement for it. Implement discovery first so every reader addresses the
correct dump, then the direct readers, then the G estimator naming and
conditional uncertainty calculation. Keep each addition covered by a small
real-format fixture and compare representative values to the original CSV or
text file.

Unresolved limits before scientific use: the configured Geant4 11.4.1 source
does not match the active 11.4.3 install, so pre-chemical field semantics and
modification labels need matched-source verification; the existing executable's
exact source revision is not established by the run records; the two-event
demonstrations cannot establish convergence or independent sampling; no
multi-dump/prefixed/nested run was exercised by the notebook; and aggregate
mesoscopic data contain no per-event variance or reaction identities. The
particle reaction file covers the particle stage only, with documented
scavenger and acid-base omissions. Any comparison between particle and
mesoscopic curves must account for stage, hand-over time, species naming,
event count, and the different normalization.
