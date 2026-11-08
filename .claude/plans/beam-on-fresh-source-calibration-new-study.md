# Authorized fresh calibration study

The interrupted 04-study and rewritten-control incident remain preserved and excluded from this new study. This is a separately declared experiment, not a resumed clock or replacement of individual failed attempts.

Use the approved scientific design in beam-on-fresh-source-calibration.md and CalibratedBeamOnProtocol.md unchanged: 1024 calibration plus 256 holdout attempts per condition, 128 independent fresh realizations per condition paired at 100/1000 Gy/s, 5% relative 95% calibration interval half-width target. Complete zero deposits remain included; truncated/failed events remain recorded without replacement.

New additional budget: at most 4608 physical capture attempts, 512 chemistry histories and 256 schedule jobs, within one new four-hour wall-clock window beginning at the recorded scientific launch. Per-process limits remain 512 MiB, at most60 seconds and3GiB free memory. No extensions if target or completion is missed. All13 physical outputs from the interrupted run remain separate; none enter the new calibration/holdout/fresh sets.

Use new directories under .scratch/tests/2026-11-08__beam-on-fresh-source-calibration: 05-preflight for controls and05-study for scientific evidence. Reject either directory if it already exists; do not silently choose a replacement. Preflight uses New-CalibratedPreflightPlan and explicit resolved paths, with all29 destinations checked before creation and checked again immediately before launch under exclusive ownership. Record fresh control output hashes before further work. Never modify prior serialized arguments or write under03-native-controls or04-study.

The runner now inventories earlier same-session streams.tsv registries as prior assignments. Reserve all04-study assigned seeds (including unexecuted slots), all prior inventories and new control seeds before freezing the new manifest. A synthetic planner check allocated14594 new streams against14594 old assignments with zero overlap; this is validation evidence only, not the final scientific manifest. Verify final manifest disjointness before the first capture.

Freeze declaration, protocol, source/binary/runtime hashes, complete seed manifest and expected outputs before scientific launch. Reuse the reviewed private runtime only after provenance checks. Use explicit bounded commands and preserve every result. After completion/deadline, audit and report calibration/holdout and paired uncertainty where supported; no complete-ensemble inference from survivors. Obtain independent result review before local delivery. No push or PR.

User explicitly authorized this separately declared fresh study and its additional fixed budget on 2026-11-08, with delegated execution and local reporting. No push or PR; all original evidence remains preserved.

