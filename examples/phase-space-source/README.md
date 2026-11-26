# phase-space-source

Standalone Geant4-DNA example: simulate the physical stage of an electron beam
in a water phantom once, save the particles that enter a small scoring box at
depth in a reusable **Phase-space file**, and replay that file in several
Geant4-DNA physics + chemistry runs of the box. The same histories are
replayed in the same order with the same physics in every run, so chemistry
variants can be compared history by history and a run can be repeated exactly.

Design records: `docs/adr/0014-phase-space-crossing-rule.md` (every inward
crossing is recorded, stage 1 kills nothing, the Scoring lattice) and
`docs/adr/0015-per-history-seeding.md` (stage 2 reseeds per history). Glossary:
Phase-space file, Phase-space history, Inward crossing, Scoring lattice in
`CONTEXT.md`.

The example copies the code it needs (`DataNode`/`JsonWriter` from PFLASH
`src/scoring/`) and links nothing from PFLASH `sim` or the other examples; a
change there does not reach this example.

Status: ticket 01. The Geant4-free core and `phsp-dump` are complete. Stage 1
(`phsp-record`) and stage 2 (`phsp-replay`) come in later tickets.

## Core library (`phspCore`, Geant4-free)

Standard library only, so its tests run without a Geant4 kernel.

- `PhaseSpaceFormat.hh`: the binary format v1 (layout below), its
  little-endian encoders and decoders, validation, FNV-1a 64 and splitmix64.
- `PhaseSpaceWriter.hh`: `WritePhaseSpaceFile` writes `<name>.phsp` and the
  `<name>.json` sidecar from histories sorted by stage-1 event ID.
- `PhaseSpaceReader.hh`: opens and fully validates one file, then reads one
  history's records on demand. Safe to share between threads.
- `HistoryCatalog.hh`: global history numbering over an ordered list of files,
  and range selection.
- `HistorySeed.hh`: the per-history seed of stage 2.
- `LatticeFold.hh`: lattice box index and box-frame position.

Errors are `std::runtime_error` with a message naming the file and the
problem; the Geant4 executables turn them into `G4Exception`.

## Phase-space file, format v1

`<name>.phsp` holds three blocks. Every field is little-endian with a fixed
width and is encoded field by field, not by copying a struct. Units are eV,
nm and ns.

| Block | Size | Content |
|---|---|---|
| File header | 128 bytes | magic `PFPHSP\r\n`, format version, block sizes, file ID, content digest, stage-1 seed, primaries fired, histories written, records written, lattice pitch, box side, depth, header checksum |
| History table | 40 bytes per history | history index, stage-1 event ID, first record, record count, in-box deposit (eV) |
| Records | 80 bytes per record | PDG, track ID, kinetic energy, position (box frame), direction, time, weight |

The byte offsets are listed in `include/PhaseSpaceFormat.hh`.

- One record is one Inward crossing, in the frame of the box it entered:
  origin at the box centre, the phantom's axes.
- One history holds all records of one stage-1 primary. Histories are written
  in stage-1 event ID order and indexed 0..N-1. A primary with no record is
  not written; it counts only in `primariesFired`.
- `contentDigest` is FNV-1a 64 over the history table and the records, in
  file order. The header has its own checksum (FNV-1a 64 over bytes 0..95),
  because the digest does not cover it.
- `fileId = SplitMix64(stage1Seed ^ SplitMix64(contentDigest))`. For the same
  content, distinct seeds always give distinct IDs. For different contents a
  clash has probability about 2^-64. The same file written twice keeps its ID,
  so a copy is detected as a duplicate.
- The lattice pitch is 0 (single box) or at least the box side, so boxes do
  not overlap.

The reader refuses a file with a wrong magic, an unsupported version, other
block sizes, a header checksum mismatch, a size that does not match the
header counts (truncated or with trailing bytes), a content digest mismatch,
an inconsistent history table, or an invalid record. Each refusal has a clear
message and none of them crashes.

The `.json` sidecar is informational: it repeats the header (IDs as
`"0x..."` strings), adds the total in-box deposit and the file size, and
holds what stage 1 passes under `"stage1"` (beam, physics). The reader uses
the binary file only.

## History numbering and ranges

Stage 2 opens an ordered list of files. The global index runs over file
order, then history index. With files A (2 histories), B (0) and C (3), A's
histories are 0-1 and C's are 2-4. The same list in the same order always
gives the same numbering. `Select(first, count)` past the end is an error, and
so is a file ID given twice (the same file listed twice, or a copy of it).

## Per-history seed (ADR 0015)

```
HistorySeed(b, f, i) = S(S(S(b) ^ f) ^ i)        S = splitmix64
```

Here `b` is the stage-2 base seed, `f` the file ID and `i` the history index.
`HistoryEngineSeeds` returns `{bits 32..62, bits 0..30, 0}` (0 replaced by 1),
for `G4Random::setTheSeeds`. MixMax (the Geant4 default) and Ranecu read the
first two values. MixMax keeps the pointer it was given, so the array must
outlive the call. The mix is fixed: `HistorySeedTest` pins its values,
because changing it changes every stage-2 result.

## Lattice fold

With pitch P > 0, box (ix, iy) is centred at (ix P, iy P, zc) and owns the
lateral cell [(ix - 1/2) P, (ix + 1/2) P) x [(iy - 1/2) P, (iy + 1/2) P). A
position on a cell edge belongs to the box on the + side. The box-frame
position is (x - ix P, y - iy P, z - zc), with x and y in [-P/2, P/2). P = 0
is the single box (0, 0).

## phsp-dump

```
phsp-dump <file.phsp> [--section all|header|histories|records]
```

`phsp-dump` validates the file, then prints the header (`field,value`), the
history table and every record as CSV. With `all` (the default), each table
follows a `# <name>` line and a blank line separates them. One section prints
that table alone, ready for a CSV reader. Reals print in the shortest form
that reads back to the same double. Exit status: 0 on success, 1 on a file
error, 2 on a usage error.

## Build and test

Build RelWithDebInfo against the stock Geant4 11.4.3 install: a Debug build
crashes on the first `G4String` against the release install. Configure from
the MSVC x64 environment (`.claude/geant4-instructions.md`), with the vcpkg
toolchain, which provides the ZLIB and HDF5 that the Geant4 config looks for:

```
cmake -S examples/phase-space-source -B examples/phase-space-source/build -G Ninja `
      -DCMAKE_BUILD_TYPE=RelWithDebInfo `
      -DCMAKE_TOOLCHAIN_FILE=$env:DEV_DIR/vcpkg/scripts/buildsystems/vcpkg.cmake `
      -DGeant4_DIR=$env:G4_ROOT/lib/cmake/Geant4
cmake --build examples/phase-space-source/build
ctest --test-dir examples/phase-space-source/build --output-on-failure
```

The tests use `test/TestCheck.hh`, which stays active under `NDEBUG`, unlike
`assert`.

| Test | Checks |
|---|---|
| `PhaseSpaceFormatTest` | byte layout, encode/decode round trips, FNV-1a and splitmix64 reference values, header and record validation |
| `PhaseSpaceFileTest` | writer + reader round trip, sidecar, writer refusals, truncated and corrupted files |
| `HistoryCatalogTest` | global numbering, ranges past the end, duplicate file ID, concurrent reads |
| `HistorySeedTest` | pinned seed values, engine-seed range |
| `LatticeFoldTest` | cell edges, negative cells, rounding, pitch 0 |
| `PhspDump*` | `phsp-dump` output, a corrupted file and the usage message |
