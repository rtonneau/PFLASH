# Dose-rate investigations

- [Next: reusable timed-source IRT_syn example](working-example-handoff.md) — current user priority, completed-work checkpoints, implementation reuse and practical delivery criteria.
- [Poisson-track IRT_syn example: integration guide](../../examples/poisson-track-irt/README.md) — standalone example, required Geant4 patches, build and run commands, hooks and known limits.
- [Matched intertrack result](beam-on-intertrack-result.md) — completed shared-versus-independent comparison; coupling remains unresolved in the tested regime.
- [Continuous-beam G-values vs dose rate (0.1–1000 Gy/s)](continuous-beam-gvalues.md) — 100 keV e⁻, 5 µm box, IRT_syn to 1 ms: no effect at 1 Gy/s, onset at 10 Gy/s, strong at 100–1000 Gy/s.
- [Dose-rate study: 4 µm box up to 1 Gy (0.1–10⁴ Gy/s)](dose-rate-study.md) — `examples/dose-rate-study`, 500 keV e⁻, reflecting box, IRT_syn to beam-off: G(t) and G at beam-off per rate, costs, snapshot movies, counter-crash limitation.

- [Recurring chemistry sources](recurring-chemistry-sources.md) — bounded particle reference.
- [Mesoscopic recurring-source feasibility](mesoscopic-feasibility.md) — bounded coupling experiment, limitations and NO-GO decision.
- [Restricted spatial coupling admission gate](restricted-coupling-feasibility.md) — native particle retention experiment, geometric sensitivity and bounded NO-GO; chemical handover convergence remains untested.
- [Bounded beam-on IRT_syn reference](beam-on-irt.md) — isolated 4 µm beam-on feasibility reference, source contract, runtime validation and the Study04/05 incident history.
- [Fresh-source calibration study (Study06)](beam-on-fresh-source-calibration.md) — accepted result: complete and audited, both 5% calibration targets missed, comparisons exploratory, attribution to inter-track chemistry unproven.
- [Handoff after Study06](beam-on-fresh-source-calibration-handoff.md) — commits, provenance, evidence locations, integrity and reproduction commands, inputs reusable for matched independent-track controls, open questions and resource limits.
