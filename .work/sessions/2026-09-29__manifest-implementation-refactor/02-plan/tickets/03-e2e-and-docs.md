# Ticket 03: e2e-and-docs

**Acceptance Criteria:**
- [ ] A `sim` run (10 keV e-, `/run/beamOn 2`) writes a `Manifest.json` that Python's `json` module parses, with:
  - exactly the 18 top-level keys in ticket 02's order;
  - run keys `[run, events, particle, beamEnergy_keV, position_um, direction, energyDeposit_eV, seed]`;
  - `totalEvents` equal to the sum of the runs' `events`;
  - 3-element `position_um`/`direction`.
- [ ] ADR 0005 paragraph 3 says:
  - the collector builds a format-neutral ordered `DataNode` tree (top-level keys in `RunManifest::Write`, run entries in `RunManifest::RecordRun`);
  - the portable `JsonWriter` serialises any tree, so adding or removing an entry never touches the writer;
  - the layout puts all-scalar containers inline and indents the others.
- [ ] The `CLAUDE.md` Key Files entries (the `ManifestWriter` line, `RunAccumulator`, `RunManifest`) and `.claude/skills/sim-output/SKILL.md` describe `DataNode` + `JsonWriter`, the `RunAccumulator` run entries and event total, and `RunManifest::RecordRun`. Neither mentions `ManifestWriter`, `ManifestData` or `RunRecord`.

**Files to Touch:**
- `docs/adr/0005-manifest-per-dump.md`
- `CLAUDE.md`
- `.claude/skills/sim-output/SKILL.md`
- (scratch macro, if needed) `.scratch/tests/2026-09-29__manifest-implementation-refactor/`

**Verification Step:**

Run (from the `build/` run dir; use `beam.in` if it's 10 keV / `beamOn 2`, otherwise the scratch macro):
```bash
./sim beam.in --dir ../.scratch/tests/2026-09-29__manifest-implementation-refactor/out
python -c "import json,sys; m=json.load(open(sys.argv[1])); print(list(m)); print(list(m['runs'][0])); print(m['totalEvents'], sum(r['events'] for r in m['runs']))" <out>/<dump>/Manifest.json
grep -nE "ManifestWriter|ManifestData|RunRecord" CLAUDE.md .claude/skills/sim-output/SKILL.md docs/adr/0005-manifest-per-dump.md
```

Expected:
- the first line lists the 18 keys in order;
- the second line lists the 8 run keys;
- the third line shows two equal numbers;
- grep prints nothing.

**Notes:**

Find the dump location with the `sim-output` skill (the `EndOfRun_` prefix applies if the macro doesn't dump). `docs/visual/app-workflow-files.svg` isn't tracked by git; leave it alone. Commit: `docs: describe DataNode/JsonWriter manifest pipeline`.
