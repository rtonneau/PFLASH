# Ticket 01: write-g4utils-work-report

**Model:** medium
**Model (Jev):** sonnet-5.5 (confidence 0.08)
**Effort:** high

**Acceptance Criteria:**
- [ ] `notebooks/G4UTILS_REPORT.md` documents the executed vertical slice, the
  G4Utils `a456290` baseline, existing coverage, and every priority gap with
  evidence, a proposed interface and return data, units/normalization,
  missing-data behavior, and testable acceptance checks.
- [ ] The report covers ResultsIndex/prefix/subdirectory discovery;
  physical-interaction, track-length, pre-chemical, and aggregate mesoscopic
  readers; aggregate-energy versus mean-event G; the SEM condition;
  library/notebook scope; and unresolved verification limits.
- [ ] `notebooks/PLAN.md` marks section 3 complete and links to the separate
  report, without marking sections 4 or 5 complete.
- [ ] Only `notebooks/G4UTILS_REPORT.md` and `notebooks/PLAN.md` are changed
  by the ticket.

**Files to Touch:**
- `notebooks/G4UTILS_REPORT.md`
- `notebooks/PLAN.md`

**Verification Step:**

Run:
```bash
pwsh -NoProfile -Command '& { $report = Get-Content -Raw notebooks/G4UTILS_REPORT.md; $required = @("ResultsIndex", "Physical interactions", "Track lengths", "Pre-chemical", "SpeciesMeso", "mean event G", "unresolved"); foreach ($term in $required) { if ($report -notmatch [regex]::Escape($term)) { throw "Missing report topic: $term" } }; if ((Get-Content -Raw notebooks/PLAN.md) -notmatch "\[x\].*G4UTILS_REPORT") { throw "PLAN does not record the separate report" }; git diff --check; Write-Output "Report structure and PLAN progress: PASS" }'
```

Expected:
`Report structure and PLAN progress: PASS` and no `git diff --check` output.

**Notes:**

Create a report only; do not change G4Utils or notebook code.  Keep current
behavior and proposed work clearly separate.  Do not claim Geant4-source
validation of pre-chemical field semantics while the configured 11.4.1 source
and active 11.4.3 install remain mismatched.
