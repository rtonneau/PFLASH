# Switching to the patched Geant4 build

The default build is stock (`PTIRT_PATCHED_GEANT4=OFF`). Use this checklist to build
and run against a patched Geant4 11.4.3 instead. Why you might want to: the ledger of a
stock run shows `newOldContacts.pairs` above 0, the packet must be taken before the
startup contacts, or you want the patched stepper to do the contact trials
(ADR 0010, `docs/adr/0010-poisson-track-irt-stock-build.md`, "Conditions that require
the patched build").

The patch recipe is unchanged and is in the example README ("Geant4 patches"). Do not
patch your shared Geant4.

## Checklist

1. Build a private patched install of Geant4 11.4.3 from a copy of the source, applying in
   order `patches/geant4/11.4.3/molecular-finder/coincident-neighbors.patch`,
   `timed-injection/source-contacts-experimental.patch` and
   `timed-injection/meso-boundaries-experimental.patch`. Check that all nine paths
   changed (README recipe, steps 1 to 4). Geant4 must be 11.4.3: the patches are for that
   version only.
2. Configure the example in a **new** build directory with `-DPTIRT_PATCHED_GEANT4=ON`
   and `-DGeant4_DIR=<patched-geant4-install>/lib/cmake/Geant4`. Configure stops if the
   install has no `ResetSourceActivations`/`RegisterSourceActivation` (a stock install)
   and, in the other direction, if `OFF` is used with a patched install. The probe
   covers the source-contact patch only: the finder and rebound patches cannot be
   detected from headers, so a partly patched install passes it.
3. Build and run `ctest`. Expected: 42 of 42 tests pass (`evidence/09-ctest-patched.log`
   under `.scratch/tests/2026-11-10__poisson-track-irt-stock-build/`). The patched tree
   has `CaptureAtChemistryStart` and `HistoryRejectsStockLibrary` in place of the stock
   tests.
4. Put `<patched-geant4-install>/bin` first on `PATH` and set `GEANT4_DATA_DIR`. Check
   `loadedGeant4Modules` in `capture.json` or `ledger.json`: every DLL must come from the
   patched install.
5. **Capture a new library with this build.** A stock library is refused (tag
   `...-stock_...-afterStartupContacts`), and so is the reverse. Use the same
   `--first-seed` as the stock library if you want a comparison: the digests of the
   chemistry-start state agree between the builds (24 of 24 packets, `comparison/README.md`).
6. Expect these differences in the ledger: `buildMode` is `patched`, `captureStage` is
   `chemistryStart`, `firstPacketSecondTrial` is null (the patched stepper trials the
   first packet once, so there is no gate), `newOldContacts.trialledByKernel` is true
   (the counts are informational), and `kernelFold` names the patched fold.
   `newOldContactLimit` still applies in the patched build.
7. Not covered by any dedicated test: the patched finder and rebound-fold patches (only
   the example's ctest runs on that install), multithreaded runs, and the
   separated-sources check on the patched build.
