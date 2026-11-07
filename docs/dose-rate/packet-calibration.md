# Precursor capture investigation — ticket 1 incomplete

The isolated MSVC RelWithDebInfo executable builds against configured Geant4
11.4.3. CTest `PrecursorBoundary` passed (8.93 s) for one 500 keV electron
in a 0.5 mm water cube. This is a central, parallel-entry diagnostic at
z = -249.999 um, not the required uniform-face library or a calibration.
Energy cuts are disabled after initialization. No production code is edited.

The StartProcessing hook observes delayed tracks before synchronization and
stops the scheduler. It observed 8,891 precursors: 4,118 e_aq^-1,
74 H2O^-1, 516 H2O^0 and 4,183 H2O^1. Every observed track has a 1 ps
activation time. Reconstructing a temporary molecule from the same in-process
configuration pointer preserved its decay-time value. The -1000 ps value
for solvated electrons is the recorded sentinel, not a physical negative age.

This is NOT an export/import demonstration. The TSV is diagnostic only: it
omits positions and electronic occupancies. In particular, configuration
names are definition plus charge (G4MolecularConfiguration.cc, lines 441–443),
so a name cannot distinguish all electronic states. Reusing configuration
pointers also cannot establish fresh-process reconstruction. A versioned
format must preserve full electronic occupancy/configuration identity,
status, position and timing and verify reconstruction in a fresh process.

Source boundary evidence: G4DNAChemistryManager.cc 631–662 creates modified
water with fStopButAlive at 1 ps; G4Scheduler.cc invokes StartProcessing before
delayed-list synchronization; G4DNAMolecularDissociation.cc 108–112 uses decay
time minus proper time and 258–260 creates alive products at parent global
time. The project also registers electron-hole recombination. Consequently
radical-only startup export is invalid.

Verification command (with Geant4 and vcpkg bin directories on PATH):

    ctest --test-dir .scratch/tests/2026-11-06__timed-track-injection-dose-rate-feasibility/01-msvc -L injection-packets --output-on-failure --no-tests=error

One diagnostic passed. Ticket acceptance has NOT passed. Remaining work:
full packet serialization and rejection tests, fresh-instance import and
chemical controls, uniform-face generation, all-secondary volume energy
scoring and abortion checks, independent library/holdout sampling, and
source-only Poisson/calibration tests. No chemistry equivalence, calibrated
rate, or feasibility conclusion is claimed. Ticket 2 must not begin.
