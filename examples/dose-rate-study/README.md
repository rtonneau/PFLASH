# dose-rate-study

Standalone Geant4-DNA example for the dose-rate study of a 4 um water box
(ADR 0011, `docs/adr/0011-reflecting-box-for-dose-rate-study.md`). It is copied
from `examples/poisson-track-irt` (packet format, capture, history, source
injection, reflecting box) and from the dnachem-min `PureWater` Chemistry
(`PureWaterReactions.cc`, `ChemistryTypes.hh`). It does not link or include
either; a change there does not reach this example.

Status: ticket 01 skeleton. `capture` is complete; `history` loads a packet
library and runs one bounded history.

## Physics and chemistry

- `G4EmDNAPhysics_option2`, Serial run manager.
- `PureWaterChemistryList`: the molecules and water dissociation of
  `G4ChemDissociationChannels_option1`, the copied PureWater reaction network,
  and the pH 7 acid-base buffer (`H3Op(B)`, `OHm(B)`, as per-molecule
  `G4DNAScavengerProcess` with a `G4DNAScavengerMaterial`). No O2.
- IRT_syn time-step model only, no mesoscopic stage.
- The bulk buffer has its own reservoir (500 um half side, the dnachem-min
  default), not the 4 um box. Geant4 rounds the bulk molecule count down to
  `floor(N_A c V)` and computes the propensity from it. In a (4 um)^3 box,
  pH 7 gives 3 H3O+ instead of 3.85, so the buffer rates would be 22 % low.
  Bulk species are never consumed, so the reservoir sets only the rates.

## capture

Each event shoots one 500 keV e- along +z. The primary starts 1 nm inside the
-z face of a 20 um water world (half side 10 um), at a uniform random (x, y)
over the face of the 4 um capture box (half side 2 um) centred in it.

Each primary becomes one `PrecursorPacket`, holding:

- the molecules strictly inside the box at 1 ps;
- the energy deposited inside the box, whichever particle deposits it, so
  delta rays born upstream count;
- `escapedEnergy`, the rest of the primary energy (deposited outside the box
  or carried out of the world).

Positions are relative to the box centre. Some primaries launched near the box
edge scatter laterally upstream and miss the box (no molecule, no deposit).
`--max-empty N` lets capture skip up to N such events; they are listed in
`capture.json` (`skippedEmptyEvents`).

```
capture --macro macros/capture.mac --output <dir> --count N --first-seed S [--max-empty M]
capture-check <dir> [--expect-count N] [--box-half-side-nm 2000]
```

The macro commands are `/drs/capture/boxHalfSide` and
`/drs/capture/worldHalfSide` (PreInit), plus `/gun/particle` and
`/gun/energy`. capture overrides the gun position and direction.
`capture.json` records `meanBoxDeposit_eV`, plus the deposit outside the box,
the world escape and the dropped molecules, for every packet and in total.

## history

`history --config <file> --output <dir>` takes the config keys of
`include/HistoryConfig.hh`. Set `halfBoxUm = 2` for the reflecting 4 um box.

## Build and test

Build against the patched Geant4 11.4.3 install, and pass its path on the
command line only. With MSVC, use the same build type as that install
(RelWithDebInfo against the Release install). A Debug build crashes at the
first `G4String` passed to Geant4, because the debug and release STL layouts
differ.

```
cmake -S examples/dose-rate-study -B examples/dose-rate-study/build-ninja -G Ninja `
      -DCMAKE_BUILD_TYPE=RelWithDebInfo -DGeant4_DIR=<patched install>/lib/cmake/Geant4
cmake --build examples/dose-rate-study/build-ninja
ctest --test-dir examples/dose-rate-study/build-ninja --output-on-failure
```

The tests:

- Unit tests of the Geant4-free core.
- `CaptureSmoke`: 20 packets. It checks the count, that every molecule is
  inside the box and that the mean box deposit is positive, and prints the
  mean box deposit.
- `HistorySmoke`: two packets of that library in the 2 um reflecting half box,
  to 100 ns.
