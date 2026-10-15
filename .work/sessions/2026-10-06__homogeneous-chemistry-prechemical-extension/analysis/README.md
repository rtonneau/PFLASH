# Tonneau2025 G-value check at 100 ns

Run: `macro/beam_tonneau2025.in` (10 keV e-, 2 events, `Tonneau2025`, hand-over time 100 ns), then
`python -I compare_gvalues.py <outdir>`.

Paper values: Table 3 of Tonneau et al. 2025, G at 100 ns for 1.17 keV/um, read from the PDF text
extraction (the page image could not be rendered here; the order of the six values after the LET
row is e_aq 2.49, H3O+ 2.49, OH 2.62, H 0.59, H2 0.31, H2O2 0.54, consistent with Boscolo 2020 and
with the paper's statement that H3O+ is set equal to e_aq).

Result (2 events, one seed, statistics are poor; mean G per event):

| species | G sim | G paper | ratio |
|---|---|---|---|
| e_aq | 1.65 | 2.49 | 0.66 |
| H3Op | 2.42 | 2.49 | 0.97 |
| OH | 1.81 | 2.62 | 0.69 |
| H | 0.91 | 0.59 | 1.54 |
| H2 | 0.74 | 0.31 | 2.39 |
| H2O2 | 0.69 | 0.54 | 1.28 |

Strong deviation: H2 (about 2.4 times the paper). e_aq and OH are about 30 % low, H and H2O2 high.
This is expected in direction: a 10 keV electron has a higher LET than the 1.17 keV/um column, and the
tracked network also acts during the first 100 ns (the paper starts its ODE at 100 ns from Boscolo
values). The comparison is indicative, not an exact reproduction. H3O+ is not reduced like e_aq
because the acid-base reactions feed it.
