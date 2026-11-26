# Stage-2 replay reseeds per phase-space history

_ADR 0015, recorded 2026-11-26 (session 2026-11-26__phase-space-source)._

<!-- Format adapted from mattpocock-skills' domain-modeling (ADR-FORMAT.md): https://github.com/mattpocock/skills.git -->

Several chemistry-stage runs (pH, dissolved O2, end time) must be able to start from the same phase-space files, and a repeated run must reproduce the same tracks in the same order. So stage 2 reseeds the engine at each event from (stage-2 base seed, file ID, history index) instead of relying on the run seed and the event order. The same history then gets the same physical stage in any range split, under any thread count and across chemistry variants (chemistry draws come after physics within an event). The price is that results no longer depend on `/random/setSeeds` alone. The run manifest has to record the base seed, the file IDs and digests, and the ranges used.
