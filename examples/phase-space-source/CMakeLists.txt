# Standalone example: phase-space source for Geant4-DNA (see README.md).
# Stage 1 (phsp-record) records every inward crossing of a scoring box into a
# Phase-space file; stage 2 (phsp-replay) replays its histories in a DNA box.
# This project copies the code it needs (DataNode/JsonWriter from PFLASH
# src/scoring/, PureWaterReactions/ChemistryTypes from PFLASH src/chemistry/)
# and links nothing from PFLASH `sim` or the other examples.
cmake_minimum_required(VERSION 3.16)
project(PhaseSpaceSource LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Geant4_DIR must point at the stock Geant4 11.4.3 install. Only the stage-1 and
# stage-2 executables use it; the core library below never includes Geant4, so
# its tests run without a kernel. Build RelWithDebInfo: a Debug build against
# the release install crashes.
find_package(Geant4 11.4.3 REQUIRED)
find_package(Threads REQUIRED)

# ---- Geant4-free core ---------------------------------------------------------
# Phase-space file format v1 (writer, reader), global history numbering and
# ranges, per-history seeds, lattice fold, JSON sidecar.
add_library(phspCore STATIC
  src/PhaseSpaceFormat.cc
  src/PhaseSpaceWriter.cc
  src/PhaseSpaceReader.cc
  src/HistoryCatalog.cc
  src/HistorySeed.cc
  src/LatticeFold.cc
  src/DataNode.cc
  src/JsonWriter.cc)
target_include_directories(phspCore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(phspCore PUBLIC Threads::Threads)
if(MSVC)
  target_compile_options(phspCore PUBLIC /W4 /utf-8)
else()
  target_compile_options(phspCore PUBLIC -Wall -Wextra)
endif()

# phsp-dump: print a phase-space file (header, history table, records) as CSV.
add_executable(phsp-dump src/PhspDumpMain.cc)
target_link_libraries(phsp-dump PRIVATE phspCore)

# ---- Geant4 application: phsp-record (stage 1) --------------------------------
# Water phantom + scoring box(es), each in a DNA_Opt2 shell, G4GeneralParticleSource
# beam; writes every inward crossing of a box into one phase-space file.
add_executable(phsp-record
  src/RecordMain.cc
  src/RecordDetectorConstruction.cc
  src/RecordPhysicsList.cc
  src/RecordActions.cc
  src/RecordMessenger.cc)
target_include_directories(phsp-record SYSTEM PRIVATE ${Geant4_INCLUDE_DIRS})
target_link_libraries(phsp-record PRIVATE phspCore ${Geant4_LIBRARIES})

# ---- Geant4 application: phsp-replay (stage 2) -----------------------------------
# A water box of the files' box side (region DnaRegion, stage-1 physics setup by
# default); one Phase-space history per event, reseeded per history (ADR 0015),
# then (/phsp/chem/enable true, the default) an IRT_syn chemistry stage with the
# copied PureWater network, the pH buffer and optional dissolved O2.
# RecordPhysicsList.cc is shared so that the stage-1 cuts and DNA option come
# from one place.
add_executable(phsp-replay
  src/ReplayMain.cc
  src/ReplayDetectorConstruction.cc
  src/ReplayPhysicsList.cc
  src/ReplayChemistryList.cc
  src/PureWaterReactions.cc
  src/PhaseSpaceSource.cc
  src/ReplayActions.cc
  src/ReplayMessenger.cc
  src/RecordPhysicsList.cc)
target_include_directories(phsp-replay SYSTEM PRIVATE ${Geant4_INCLUDE_DIRS})
target_link_libraries(phsp-replay PRIVATE phspCore ${Geant4_LIBRARIES})

# ---- Unit tests (TestCheck.hh: active in every build type) -------------------
enable_testing()
set(phspTestOutput ${CMAKE_CURRENT_BINARY_DIR}/test-output)
foreach(testName PhaseSpaceFormatTest HistorySeedTest LatticeFoldTest)
  add_executable(${testName} test/${testName}.cc)
  target_link_libraries(${testName} PRIVATE phspCore)
  add_test(NAME ${testName} COMMAND ${testName})
endforeach()

add_executable(PhaseSpaceFileTest test/PhaseSpaceFileTest.cc)
target_link_libraries(PhaseSpaceFileTest PRIVATE phspCore)
add_test(NAME PhaseSpaceFileTest COMMAND PhaseSpaceFileTest ${phspTestOutput}/phase-space-file)
set_tests_properties(PhaseSpaceFileTest PROPERTIES FIXTURES_SETUP PhspRoundTripFile)

add_executable(HistoryCatalogTest test/HistoryCatalogTest.cc)
target_link_libraries(HistoryCatalogTest PRIVATE phspCore)
add_test(NAME HistoryCatalogTest COMMAND HistoryCatalogTest ${phspTestOutput}/history-catalog)

# ---- phsp-dump on the round-trip file of PhaseSpaceFileTest -------------------
# Output line ends are CR LF on Windows, hence `.*` between the expected lines.
set(phspRoundTrip ${phspTestOutput}/phase-space-file/roundtrip.phsp)
add_test(NAME PhspDumpAll COMMAND phsp-dump ${phspRoundTrip})
set_tests_properties(PhspDumpAll PROPERTIES
  FIXTURES_REQUIRED PhspRoundTripFile
  PASS_REGULAR_EXPRESSION "# header.*formatVersion,1.*historiesWritten,3.*recordsWritten,6.*# histories.*1,3,-1,1,2,1,0.*2,11,0,0,3,3,98765.4321.*# records.*2,11,0,0,5,11,12,0,5,-1.6666666666666667,-2000,0,0.6,0.8,")
add_test(NAME PhspDumpRecords COMMAND phsp-dump ${phspRoundTrip} --section records)
set_tests_properties(PhspDumpRecords PROPERTIES
  FIXTURES_REQUIRED PhspRoundTripFile
  PASS_REGULAR_EXPRESSION "^historyIndex,stage1EventId,boxIx,boxIy,recordIndex,pdg,trackId,kineticEnergyEv,xNm,yNm,zNm,directionX,directionY,directionZ,timeNs,weight.*0,3,0,0,0,11,1,490000,10,"
  FAIL_REGULAR_EXPRESSION "# |field,value")
add_test(NAME PhspDumpCorrupted COMMAND phsp-dump ${phspTestOutput}/phase-space-file/flipped-record.phsp)
set_tests_properties(PhspDumpCorrupted PROPERTIES
  FIXTURES_REQUIRED PhspRoundTripFile
  PASS_REGULAR_EXPRESSION "phsp-dump: phase-space file .*content digest mismatch")
add_test(NAME PhspDumpUsage COMMAND phsp-dump)
set_tests_properties(PhspDumpUsage PROPERTIES PASS_REGULAR_EXPRESSION "usage: phsp-dump")

# ---- phsp-record (runs the Geant4 kernel) --------------------------------------
# RecordSmoke: a small lattice run (test/RecordSmoke.mac: 1 mm phantom, box at
# 0.2 mm, pitch 20 um) with 1 and 2 threads and the same seed; the two .phsp
# and .json files must be byte-identical; phsp-dump must read the file and the
# header counts must agree with the history table.
add_test(NAME RecordSmoke
  COMMAND ${CMAKE_COMMAND}
    -DRECORD=$<TARGET_FILE:phsp-record>
    -DDUMP=$<TARGET_FILE:phsp-dump>
    -DMACRO=${CMAKE_CURRENT_SOURCE_DIR}/test/RecordSmoke.mac
    -DOUTPUT_DIR=${phspTestOutput}/record-smoke
    -DSEED=20261126
    -P ${CMAKE_CURRENT_SOURCE_DIR}/test/RunRecordSmoke.cmake)
add_test(NAME RecordUsage COMMAND phsp-record)
set_tests_properties(RecordUsage PROPERTIES PASS_REGULAR_EXPRESSION "RECORD_ERROR usage: phsp-record")
# A pitch equal to the box side (boxes sharing faces) is refused before /run/initialize.
add_test(NAME RecordRejectsTouchingBoxes
  COMMAND phsp-record ${CMAKE_CURRENT_SOURCE_DIR}/test/RecordTouchingBoxes.mac --output ${phspTestOutput}/record-touching/x)
set_tests_properties(RecordRejectsTouchingBoxes PROPERTIES
  PASS_REGULAR_EXPRESSION "RECORD_ERROR RecordTouchingBoxes.mac:[0-9]+: inconsistent geometry: latticePitch .* must be 0 .single box. or greater than boxSide")
# A pitch in (boxSide, boxSide + 2 x shell] makes neighbouring DNA shells
# overlap (5 um) or share a face (6 um): refused before /run/initialize.
set(phspRecordMacros ${CMAKE_CURRENT_BINARY_DIR}/test-macros)
set(phspRecordGeometry "/phsp/geom/boxSide 4 um
/phsp/geom/shell 1 um
")
file(WRITE ${phspRecordMacros}/RecordPitch5um.mac "${phspRecordGeometry}/phsp/geom/latticePitch 5 um
/run/initialize
/run/beamOn 1
")
file(WRITE ${phspRecordMacros}/RecordPitch6um.mac "${phspRecordGeometry}/phsp/geom/latticePitch 6 um
/run/initialize
/run/beamOn 1
")
# Commands that would hide a /random/ command from the line-by-line check (a
# nested macro, a loop, an alias, a macro argument of /run/beamOn): refused
# at their line, before /run/initialize. The nested macro holds /random/setSeeds.
file(WRITE ${phspRecordMacros}/RecordNestedSeed.mac "/random/setSeeds 1 2
")
file(WRITE ${phspRecordMacros}/RecordNestedExecute.mac "/control/execute ${phspRecordMacros}/RecordNestedSeed.mac
/run/initialize
/run/beamOn 1
")
file(WRITE ${phspRecordMacros}/RecordNestedForeach.mac "/control/foreach ${phspRecordMacros}/RecordNestedSeed.mac v 1
/run/initialize
/run/beamOn 1
")
file(WRITE ${phspRecordMacros}/RecordNestedLoop.mac "/control/loop ${phspRecordMacros}/RecordNestedSeed.mac v 1 1
/run/initialize
/run/beamOn 1
")
file(WRITE ${phspRecordMacros}/RecordNestedMacroPath.mac "/control/macroPath ${phspRecordMacros}
/run/initialize
/run/beamOn 1
")
file(WRITE ${phspRecordMacros}/RecordNestedAlias.mac "/control/alias seeds /random/setSeeds
/run/initialize
/run/beamOn 1
")
file(WRITE ${phspRecordMacros}/RecordBeamOnMacro.mac "/run/initialize
/run/beamOn 1 ${phspRecordMacros}/RecordNestedSeed.mac
")
set(phspRecordRefusedTests RecordRejectsPitch5um RecordRejectsPitch6um RecordRejectsExecute RecordRejectsForeach
                           RecordRejectsLoop RecordRejectsMacroPath RecordRejectsAlias RecordRejectsBeamOnMacro)
set(phspRecordRefusedMacros RecordPitch5um RecordPitch6um RecordNestedExecute RecordNestedForeach RecordNestedLoop
                            RecordNestedMacroPath RecordNestedAlias RecordBeamOnMacro)
set(phspRecordRefusedRegex
  "RECORD_ERROR RecordPitch5um.mac:4: inconsistent geometry: latticePitch .* must be 0 .single box. or greater than boxSide"
  "RECORD_ERROR RecordPitch6um.mac:4: inconsistent geometry: latticePitch .* must be 0 .single box. or greater than boxSide"
  "RECORD_ERROR RecordNestedExecute.mac:1: /control/execute is not allowed"
  "RECORD_ERROR RecordNestedForeach.mac:1: /control/foreach is not allowed"
  "RECORD_ERROR RecordNestedLoop.mac:1: /control/loop is not allowed"
  "RECORD_ERROR RecordNestedMacroPath.mac:1: /control/macroPath is not allowed"
  "RECORD_ERROR RecordNestedAlias.mac:1: /control/alias is not allowed"
  "RECORD_ERROR RecordBeamOnMacro.mac:2: /run/beamOn takes only the number of events")
foreach(i RANGE 7)
  list(GET phspRecordRefusedTests ${i} name)
  list(GET phspRecordRefusedMacros ${i} macro)
  list(GET phspRecordRefusedRegex ${i} regex)
  add_test(NAME ${name} COMMAND phsp-record ${phspRecordMacros}/${macro}.mac --output ${phspTestOutput}/record-refused/${macro})
  set_tests_properties(${name} PROPERTIES PASS_REGULAR_EXPRESSION "${regex}" FAIL_REGULAR_EXPRESSION "RECORD_DONE")
endforeach()
# RecordSmoke's 1-thread file is the input of the Replay* tests.
set_tests_properties(RecordSmoke PROPERTIES FIXTURES_SETUP PhspRecordSmokeFile)
set(phspSmokeFile ${phspTestOutput}/record-smoke/t1/smoke.phsp)

# ---- phsp-replay (runs the Geant4 kernel) ---------------------------------------
# ReplayRepeat: the whole RecordSmoke file replayed twice, split into two runs,
# and on 2 threads: identical Histories.csv rows; another base seed changes the
# stage-2 deposits. ReplayEnergy: stage-2 vs stage-1 mean deposit per history,
# |z| < 3 on the paired difference. Both run with /phsp/chem/enable false
# (physics only). ReplayChemVariant: the same histories with the chemistry on,
# dissolved O2 0 and 21 %: identical stage-2 deposits and pre-chemical counts
# per history, less e_aq at the end time (1 us) with O2.
foreach(check Repeat Energy ChemVariant)
  add_test(NAME Replay${check}
    COMMAND ${CMAKE_COMMAND}
      -DREPLAY=$<TARGET_FILE:phsp-replay>
      -DDUMP=$<TARGET_FILE:phsp-dump>
      -DPHSP=${phspSmokeFile}
      -DOUTPUT_DIR=${phspTestOutput}/replay-${check}
      -DBASE_SEED=20261126
      -P ${CMAKE_CURRENT_SOURCE_DIR}/test/RunReplay${check}.cmake)
  set_tests_properties(Replay${check} PROPERTIES FIXTURES_REQUIRED PhspRecordSmokeFile)
endforeach()

# ReplayMultiFile: two files [RecordSmoke, round trip]; the second file's
# histories alone, then that file alone, then the same content under another
# file ID (other-seed.phsp of PhaseSpaceFileTest): global numbering, per-history
# seed from (base seed, file ID, history index), Manifest.json content.
add_test(NAME ReplayMultiFile
  COMMAND ${CMAKE_COMMAND}
    -DREPLAY=$<TARGET_FILE:phsp-replay>
    -DDUMP=$<TARGET_FILE:phsp-dump>
    -DPHSP=${phspSmokeFile}
    -DSECOND=${phspRoundTrip}
    -DOTHER_ID=${phspTestOutput}/phase-space-file/other-seed.phsp
    -DOUTPUT_DIR=${phspTestOutput}/replay-MultiFile
    -DBASE_SEED=20261126
    -P ${CMAKE_CURRENT_SOURCE_DIR}/test/RunReplayMultiFile.cmake)
set_tests_properties(ReplayMultiFile PROPERTIES FIXTURES_REQUIRED "PhspRecordSmokeFile;PhspRoundTripFile")

# Fatal cases, each before any event runs. The macros are written here because
# they name the RecordSmoke file.
set(phspReplayMacros ${CMAKE_CURRENT_BINARY_DIR}/test-macros)
set(phspReplayHead "/control/verbose 0\n/run/verbose 0\n/process/em/verbose 0\n")
file(WRITE ${phspReplayMacros}/ReplayDuplicateFile.mac
  "${phspReplayHead}/phsp/source/addFile ${phspSmokeFile}\n/phsp/source/addFile ${phspSmokeFile}\n/run/initialize\n/run/beamOn 1\n")
file(WRITE ${phspReplayMacros}/ReplayRangePastEnd.mac
  "${phspReplayHead}/phsp/source/addFile ${phspSmokeFile}\n/run/initialize\n/phsp/source/first 1000000\n/run/beamOn 1\n")
file(WRITE ${phspReplayMacros}/ReplayOverlap.mac
  "${phspReplayHead}/phsp/source/addFile ${phspSmokeFile}\n/run/initialize\n/phsp/source/first 0\n/run/beamOn 2\n/phsp/source/first 1\n/run/beamOn 1\n")
file(WRITE ${phspReplayMacros}/ReplayBoxSideMismatch.mac
  "${phspReplayHead}/phsp/source/addFile ${phspSmokeFile}\n/phsp/geom/boxSide 5 um\n/run/initialize\n/run/beamOn 1\n")
file(WRITE ${phspReplayMacros}/ReplayBoxSideMismatchFiles.mac
  "${phspReplayHead}/phsp/source/addFile ${phspSmokeFile}
/phsp/source/addFile ${phspTestOutput}/phase-space-file/box-5um.phsp
/run/initialize
/run/beamOn 1
")
file(WRITE ${phspReplayMacros}/ReplayBadO2Unit.mac
  "${phspReplayHead}/phsp/source/addFile ${phspSmokeFile}\n/phsp/chem/o2 21 ppm\n/run/initialize\n/run/beamOn 1\n")
file(WRITE ${phspReplayMacros}/ReplayNotIrtSyn.mac
  "${phspReplayHead}/phsp/source/addFile ${phspSmokeFile}\n/process/chem/TimeStepModel SBS\n/run/initialize\n/run/beamOn 1\n")
# /run/initialize from a nested macro skips EnableChemistry: refused at the first run.
file(WRITE ${phspReplayMacros}/ReplayNestedInitSub.mac
  "/phsp/source/addFile ${phspSmokeFile}\n/run/initialize\n/run/beamOn 1\n")
file(WRITE ${phspReplayMacros}/ReplayNestedInit.mac
  "${phspReplayHead}/control/execute ${phspReplayMacros}/ReplayNestedInitSub.mac\n")
set(phspReplayFatalTests ReplayRejectsDuplicateFile ReplayRejectsRangePastEnd ReplayRejectsOverlap
                         ReplayRejectsBoxSideMismatch ReplayRejectsBoxSideMismatchFiles ReplayRejectsBadO2Unit ReplayRejectsNotIrtSyn
                         ReplayRejectsNestedInitialize)
set(phspReplayFatalMacros ReplayDuplicateFile ReplayRangePastEnd ReplayOverlap ReplayBoxSideMismatch
                          ReplayBoxSideMismatchFiles ReplayBadO2Unit ReplayNotIrtSyn ReplayNestedInit)
set(phspReplayFatalRegex
  "PhspReplay003.*given twice"
  "PhspReplay007.*goes past the end"
  "PhspReplay008.*overlap histories 0..1 already replayed"
  "PhspReplay005.*boxSide 5 um differs from the box side of the phase-space files .4 um."
  "PhspReplay004.*box-5um.phsp' .position 1. has box side 5 um.*every file of a replay must share the box side"
  "PhspReplay022.*unknown O2 unit 'ppm'"
  "PhspChem010.*IRT_syn only"
  "PhspReplay034.*chemistry was not built")
foreach(i RANGE 7)
  list(GET phspReplayFatalTests ${i} name)
  list(GET phspReplayFatalMacros ${i} macro)
  list(GET phspReplayFatalRegex ${i} regex)
  add_test(NAME ${name}
    COMMAND phsp-replay ${phspReplayMacros}/${macro}.mac --output ${phspTestOutput}/replay-fatal/${macro})
  set_tests_properties(${name} PROPERTIES
    FIXTURES_REQUIRED PhspRecordSmokeFile
    PASS_REGULAR_EXPRESSION "${regex}"
    FAIL_REGULAR_EXPRESSION "REPLAY_DONE")
endforeach()
set_tests_properties(ReplayRejectsBoxSideMismatchFiles PROPERTIES FIXTURES_REQUIRED "PhspRecordSmokeFile;PhspRoundTripFile")
add_test(NAME ReplayUsage COMMAND phsp-replay)
set_tests_properties(ReplayUsage PROPERTIES PASS_REGULAR_EXPRESSION "REPLAY_ERROR usage: phsp-replay")

if(WIN32)
  # The Geant4 DLLs are in <install>/bin.
  get_filename_component(phspGeant4BinDir "${Geant4_DIR}/../../../bin" ABSOLUTE)
  set_tests_properties(RecordSmoke RecordUsage RecordRejectsTouchingBoxes ${phspRecordRefusedTests} ReplayRepeat
                       ReplayEnergy ReplayChemVariant ReplayMultiFile ${phspReplayFatalTests} ReplayUsage PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${phspGeant4BinDir}")
endif()
