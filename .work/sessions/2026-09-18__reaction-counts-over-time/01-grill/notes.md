# Brainstorm Transcript

Classified as bounded work (existing `TimeStepAction::UserReactionAction` stub + existing
`ScoreSpecies`/`Run`/`RunAction` scorer pattern to extend).

Clarifying questions and answers:
1. Scope: bimolecular reactions only (via `UserReactionAction`), acid-base/scavenger network
   deferred as future work — user chose the recommended option, confirmed by verifying in
   Geant4 11.4.1 source that `G4DNAScavengerProcess::PostStepDoIt` never calls
   `UserReactionAction`.
2. Binning: per-time-bin histogram counts (reactions fired within each interval), not
   cumulative running totals — user chose the recommended option.
3. Aggregation: aggregated across all events only (no per-event `_all.csv` variant) — user
   chose the recommended option.

Design presented in chat (see `01-grill/resume.md` Architecture & Approach for the full
writeup) and approved by the user, who also asked to implement fully and git commit + push.
