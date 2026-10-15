# Physical/pre-chemical stage interaction counts

Mean count per event (molecules/events), +-1 SEM across events, from
`PreChemical_run<R>_event<E>.txt`. Ionisation/excitation/attachment are H2O
lines by modification code (0/1/2); secondary_e is the count of e_aq lines
(solvated secondary electrons). See the script docstring for exact definitions.

All 4 O2-level macros in a batch share the fixed RNG seed (12345), and O2 only
acts in the chemistry stage (after these files are written), so these files are
bit-identical across O2 levels -- confirmed below. One representative O2
directory per beam is used for the counts (pooling across O2 would count the
same events repeatedly and understate SEM); the electron run uses the 300-event
batch, whose events are a seed-identical superset of the earlier 56-event batch.

## Representative counts

| Quantity | 90 MeV proton (10x10x10 um^3, 16 ev) | 500 keV electron (500 um box, eLossMin 5 keV, 300 ev) | Electron / proton ratio |
|---|---|---|---|
| Water ionisation | 206.500 +/- 50.616 (n=16) | 350.653 +/- 32.008 (n=300) | 1.70 |
| Water excitation | 27.375 +/- 5.501 (n=16) | 42.357 +/- 3.810 (n=300) | 1.55 |
| Dissociative attachment | 3.938 +/- 0.692 (n=16) | 6.497 +/- 0.585 (n=300) | 1.65 |
| Secondary e- (solvated) | 202.875 +/- 50.137 (n=16) | 345.007 +/- 31.514 (n=300) | 1.70 |

Ionisation/excitation ratio: proton 7.543, electron 8.279.

## Per O2 level (determinism check -- every row below must equal the
representative row above exactly, not just within SEM: physical stage does not
see O2, and all O2 levels share the same seed)

### Proton

| O2 level | Ionisation | Excitation | Attachment | Secondary e- |
|---|---|---|---|---|
| 0% | 206.500 +/- 50.616 (n=16) | 27.375 +/- 5.501 (n=16) | 3.938 +/- 0.692 (n=16) | 202.875 +/- 50.137 (n=16) |
| 3% | 206.500 +/- 50.616 (n=16) | 27.375 +/- 5.501 (n=16) | 3.938 +/- 0.692 (n=16) | 202.875 +/- 50.137 (n=16) |
| 7% | 206.500 +/- 50.616 (n=16) | 27.375 +/- 5.501 (n=16) | 3.938 +/- 0.692 (n=16) | 202.875 +/- 50.137 (n=16) |
| 21% | 206.500 +/- 50.616 (n=16) | 27.375 +/- 5.501 (n=16) | 3.938 +/- 0.692 (n=16) | 202.875 +/- 50.137 (n=16) |

### Electron

| O2 level | Ionisation | Excitation | Attachment | Secondary e- |
|---|---|---|---|---|
| 0% | 350.653 +/- 32.008 (n=300) | 42.357 +/- 3.810 (n=300) | 6.497 +/- 0.585 (n=300) | 345.007 +/- 31.514 (n=300) |
| 2.5% | 350.653 +/- 32.008 (n=300) | 42.357 +/- 3.810 (n=300) | 6.497 +/- 0.585 (n=300) | 345.007 +/- 31.514 (n=300) |
| 5% | 350.653 +/- 32.008 (n=300) | 42.357 +/- 3.810 (n=300) | 6.497 +/- 0.585 (n=300) | 345.007 +/- 31.514 (n=300) |
| 21% | 350.653 +/- 32.008 (n=300) | 42.357 +/- 3.810 (n=300) | 6.497 +/- 0.585 (n=300) | 345.007 +/- 31.514 (n=300) |
