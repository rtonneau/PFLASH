# Independent Study06 arrival audit

No schedule defect was found; no draw was replaced or conditioned on count.
The independent audit reconstructs all 14594 allocated Study06 streams, including
unused slots, against its frozen prior reservations. It recomputes the deposit
mean from 1024 calibration energy ledgers per condition, reconstructs intensity
from 6.4e-14 kg and 1.602176634e-13 J/MeV, and draws mt19937_64 uniforms with the
explicit 53-bit conversion. It uses -log(1-u)/lambda without importing production
schedule or seed allocation helpers. Every included arrival and the first excluded
draw is evaluated against the half-open window. Both rate schedules and normalized
rows match bit for bit. Empty realizations remain present; no source overflow was
observed (maximum six). An overflow or missing schedule blocks preparation.

| Condition | Frozen mean (MeV) | Intensity at 100 Gy/s (s^-1) | Expected total | Observed | Empty units |
|---|---:|---:|---:|---:|---:|
| water | 0.00072052303561493433 | 55439.807059682746 | 141.92590607278785 | 158 | 44 |
| oxygen | 0.0006813043343523714 | 58631.152133385578 | 150.09574946146708 | 183 | 30 |

Water count histogram N=0..5: 44,38,26,13,6,1. Oxygen N=0..6:
30,46,32,12,4,3,1. The oxygen excess is retained as the observed ensemble.

The initial independent code/results remain under
`.scratch/tests/2026-11-09__beam-on-intertrack-dose-rate-validation/arrival-audit/`:
`ArrivalAudit.cc`, `Audit.txt`, `ExecutionAudit.txt`, and `source-hashes.csv`.
The execution audit verified five frozen source hashes and all 256 distinct
accepted schedule jobs, their arguments and completion metadata. Source inspection
of RunCalibratedBeamOn, FreshSchedule, BeamSchedule and CalibratedProcess found one
frozen arrival stream per paired realization, a half-open exponential schedule,
no count conditioning, automatic retry or successful-realization filtering.

The tracked reusable audit is [ArrivalAudit.cc](../../test/intertrack/ArrivalAudit.cc).
It additionally fails closed on malformed/trailing input and reports the first
excluded draw per realization. Run `ArrivalAudit.exe <Study06/06-study>`; it writes
only to stdout. Preparation saves its output in the new evidence directory and
verifies original input bytes against Study06's archive inventory first. The
original scratch artifacts and original Study06 evidence are never rewritten.

Source check for pool accounting used configured Geant4 11.4.3
`source/processes/electromagnetic/dna/utils/src/G4DNAScavengerMaterial.cc`:
material counts are floored; initialization floors the full pool, and reduction
and addition leave water and pH buffer species unchanged. Native output names are
`H2O^0`, `H3O^1`, `OH^-1` and `O_2^0`; tracked electron output is `e_aq^-1`.
Every recorded bulk component therefore uses its own final-minus-initial delta;
tracked yields remain separately namespaced. These accessor counts are the
observables, not exact underlying continuous concentrations.
