# Ticket 07: sbs-regression

**Model:** sonnet
**Model (Jev):** sonnet-5.5 (confidence 0.43)
**Effort:** high

**Acceptance Criteria:**
- [ ] Water, SBS, N=40, 1 us, `--threads` as in the reference manifest: G(e_aq, OH, H2O2, H2, H) at 1 us within 3x the reference statistical error (`validation/reference/sbs_water/README.md`).
- [ ] O2 21 %, SBS, N=300: same comparison with the larger error bars stated.
- [ ] Short report `docs/sbs-regression.md` with the numbers, the commands, and wall times.

**Files to Touch:**
- `docs/sbs-regression.md`
- `analysis/compare_reference.py` (only if a flag is needed)

**Verification Step:**

Run:
```bash
python analysis/compare_reference.py --help
```

Expected:
The comparison script's usage; the report lists a pass/fail line per species.

**Notes:**

Run in the background (MT). Reference beam and settings are in `validation/reference/sbs_*`. A failure is reported, not hidden; if SBS differs from the reference, check the reaction table and bulk-reaction registration against `b26a262^` before concluding.
