# Ticket 04: reaction-dump-master-only

**Model:** haiku

**Acceptance Criteria:**
- [ ] In `DnaChemistryList::ConstructProcess`, the dump condition is `if (!fReactionDumpFile.empty() && !G4Threading::IsWorkerThread())`. `G4Threading.hh` is included.
- [ ] The comment above the dump says it is written once, from the master (MT) or the only thread (Serial).
- [ ] The Serial and `--threads 4` dumps are byte-identical.

**Files to Touch:**
- `src/chemistry/DnaChemistryList.cc`

**Verification Step:**

Run:
```bash
cmake --build build --config RelWithDebInfo --target sim
# build/macro/rtdump.in: /dnaLogger/verbose Info, /chem/reaction/dump rt.txt, /run/initialize, 10 keV e-, /run/beamOn 2
cd build && ./sim rtdump.in --dir ../.scratch/tests/2026-09-29__diagnostic-output-files/t4s
./sim rtdump.in --threads 4 --dir ../.scratch/tests/2026-09-29__diagnostic-output-files/t4m
diff ../.scratch/tests/2026-09-29__diagnostic-output-files/t4s/rt.txt ../.scratch/tests/2026-09-29__diagnostic-output-files/t4m/rt.txt
```

Expected:
The diff is empty, and each run's log has exactly one `[ReactionTableDump] reaction table written to` line.

**Notes:**

- The `/chem/reaction/dump` command must be issued before `/run/initialize`. Check the command's state in `DnaChemistryList.cc:114` and adjust the macro order if needed.
- The master's process table already has the bulk processes, so its dump is complete.
