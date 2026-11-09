# Handoff: reusable timed-source IRT_syn example

## User direction (2026-11-09)

The next deliverable is a working, understandable model for exploring dose-rate
effects. Demonstrating a significant scientific effect, meeting the earlier 5%
calibration target, and publication-level validation are not delivery gates.
Retain engineering correctness checks and report limitations honestly.

Build a dedicated standalone example before integrating delivery into the main
application. The existing project defaults and historical scope remain unchanged.
Beam-off mesoscopic chemistry is deferred.

## Completed work to preserve

- Preparation and calibration handoff: `0096d01`,
  [calibration report](beam-on-fresh-source-calibration.md) and
  [input/provenance handoff](beam-on-fresh-source-calibration-handoff.md).
- Matched-control implementation: `e4fe3f7`.
- Accepted bounded intertrack report: `a37babe`,
  [result](beam-on-intertrack-result.md),
  [arrival audit](beam-on-intertrack-arrival-audit.md).
- All 24 native controls and 682 independent histories passed. The study did not
  resolve nonzero coupling; this does not prevent delivery of a working example.
  The unusual oxygen arrival count was reconstructed without finding a generator
  defect. Do not reroll or replace the accepted schedules.

Preserve all scratch evidence, original checkpoints, immutable archives, private
builds and ignored `.work/` state. Existing GPS completion is finish-pending to
avoid publication; do not run finish, push, or create a PR as cleanup.

## Reusable implementation and current limits

`test/timed-injection/probe/BeamSchedule.cc` samples exponential waiting times in
a fixed exposure window. `FiniteSchedule.cc` reads deterministic arrival lists.
`BeamOnHistory.cc` stages packet precursors into one native chemistry history,
with independent instance IDs and exact activation and energy records.

The driver currently restricts the geometry to a 4 micrometer cube, at most eight
sources, and an endpoint no later than 3 ms. That endpoint allowance is not
evidence that the proposed millisecond separated-track example has been run.
Its build and helper dependencies are research scaffolding, not a portable API.

The working architecture transports physical primaries separately, captures
their precursors, and injects those precursors into a common chemical timeline.
It does not transport new physical primaries inside a running chemistry scheduler.
Future sources are currently preloaded. Incremental streaming is not required
for a two- or three-source example.

The accepted private runtime combines the finder, source-contact and reflecting
boundary patches; consult the linked reports for exact provenance. Do not assume
the same behavior from an unpatched shared Geant4 installation.

## Next session deliverable

Create a minimal standalone C++ example and an integration guide that a different
user can follow without the prior studies or their ignored evidence directories.
Provide reproducible input generation, configuration, build and run commands,
clearly identified Geant4 patches, and explicit application integration hooks.

Provide two modes:

1. A fixed two-/three-track demonstration with configurable spatial placement,
   injection times and a few milliseconds of IRT_syn evolution. Compare the
   combined result with independently evolved packets at matching molecular ages.
2. Poisson arrivals specified directly by arrival rate and exposure duration,
   with separate source and chemistry seeds. The source count fluctuates;
   forcing two or three arrivals belongs only to the deterministic demonstration.
   Dose-rate conversion can remain an optional calibrated layer.

Choose a domain and separation appropriate to the demonstration; do not assume
independence over milliseconds in the existing small reflecting cube. Initially
avoid finite shared oxygen depletion as an unintended coupling in the separated
control. Preserve geometry and energy consistency when capturing or placing
packets. Use a small declared ensemble and practical comparison tolerance;
identical stochastic trajectories and a significant dose-rate effect are not
requirements. Include useful time-resolved output, not only final surviving counts.

Acceptance: documented commands build and run the example from its declared
inputs; sources activate exactly once at the intended times; existing chemistry
survives later injections; identities and energy accounting are correct; malformed
inputs are rejected; the separated-source comparison is explained and checked.
Run only bounded examples with declared resource limits. No full-dose production
run, shared Geant4 edits, new calibration campaign, push or PR.
