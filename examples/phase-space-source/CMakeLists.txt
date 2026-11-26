# Standalone example: phase-space source for Geant4-DNA (see README.md).
# Stage 1 (phsp-record) records every inward crossing of a scoring box into a
# Phase-space file; stage 2 (phsp-replay) replays its histories in a DNA box.
# This project copies the code it needs (DataNode/JsonWriter from PFLASH
# src/scoring/) and links nothing from PFLASH `sim` or the other examples.
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
if(WIN32)
  # The Geant4 DLLs are in <install>/bin.
  get_filename_component(phspGeant4BinDir "${Geant4_DIR}/../../../bin" ABSOLUTE)
  set_tests_properties(RecordSmoke RecordUsage RecordRejectsTouchingBoxes PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${phspGeant4BinDir}")
endif()
