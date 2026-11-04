# Table products in h5 and root

Where these files sit in the output directory: [README.md](README.md). Columns
and units of each product: the `sim-output` skill
([`.claude/skills/sim-output/SKILL.md`](../../.claude/skills/sim-output/SKILL.md)).
The nested spatial snapshot file is separate:
[`SpeciesMesoSpatial-h5.md`](SpeciesMesoSpatial-h5.md).

Every product is a set of **tables** (a table = named columns, one row per
entry). `Reactions` has two (`reactions`, `reactionCatalog`); every other
product has one, named after the product with a lower-case first letter
(`species`, `speciesMeso`, `physicsInteractions`, `trackLengths`,
`preChemical`, `speciesMesoSpatial`).

| Format | File |
|---|---|
| `csv`, `txt` | one file per table: `<Product>.<ext>` for the first table, `<Product><Name>.<ext>` for the next (`Reactions.csv`, `ReactionsCatalog.csv`) |
| `h5`, `root` | one file `<Product>.<ext>` holding all tables of the product |

`PreChemical` is the exception: txt and csv write one file per event
(`PreChemical_run<R>_event<E>.<ext>`); h5 and root write one `PreChemical.<ext>`
per dump.

## h5 layout (`Hdf5TableFile`)

```text
/                         root attribute: formatVersion (int32, currently 1)
  <tableName>/            group; attribute: description
    <columnName>          1-D dataset, one per column; attributes: unit, description
```

All datasets of a table have the same length (one entry per row). Column types:
integer columns are int64, double columns float64, string columns
variable-length UTF-8. An empty table keeps its group and zero-length
datasets. Needs a build with HDF5 (`"hdf5": true` in `Manifest.json`).

```python
import h5py
with h5py.File("Reactions.h5") as f:
    reactions = f["reactions"]
    print(reactions.attrs["description"])
    count = reactions["countSum"][:]
    unit = reactions["countSum"].attrs["unit"]
```

## root layout (`RootTableFile`)

Written with Geant4's g4tools (`tools::wroot`), so no ROOT installation is
needed to produce or to read it with uproot.

```text
<tableName>               TTree in the top directory; title = table description
  <columnName>            one branch (leaf) per column
```

- double columns are stored as double, string columns as `std::string`;
- integer columns are stored as **double** (g4tools has no 64-bit integer
  leaf); exact up to 2^53, a larger value makes the write fail instead of
  losing digits;
- the unit is part of the leaf title, `"<name> [<unit>]"` (just `"<name>"`
  when unitless); the branch title cannot be set with g4tools;
- compression is zlib level 1.

```python
import uproot
with uproot.open("Species.root") as f:
    table = f["species"].arrays(library="pd")
```

`SpeciesMesoSpatial.root` is the same layout with one long tree, see
[`SpeciesMesoSpatial-h5.md`](SpeciesMesoSpatial-h5.md#root-variant).

## Written whole at the dump

h5 and root table files are written once per dump, never appended. For
`PreChemical` and the root `SpeciesMesoSpatial` the rows of all events are held in
memory until the dump (each event's csv/txt files and the spatial h5 are
appended to disk during the run instead).
