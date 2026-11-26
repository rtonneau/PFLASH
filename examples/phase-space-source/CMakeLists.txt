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
  PASS_REGULAR_EXPRESSION "# header.*formatVersion,1.*historiesWritten,3.*recordsWritten,6.*# histories.*2,11,3,3,98765.4321.*# records.*2,11,5,11,12,0,5,-1.6666666666666667,-2000,0,0.6,0.8,")
add_test(NAME PhspDumpRecords COMMAND phsp-dump ${phspRoundTrip} --section records)
set_tests_properties(PhspDumpRecords PROPERTIES
  FIXTURES_REQUIRED PhspRoundTripFile
  PASS_REGULAR_EXPRESSION "^historyIndex,stage1EventId,recordIndex,pdg,trackId,kineticEnergyEv,xNm,yNm,zNm,directionX,directionY,directionZ,timeNs,weight.*0,3,0,11,1,490000,10,"
  FAIL_REGULAR_EXPRESSION "# |field,value")
add_test(NAME PhspDumpCorrupted COMMAND phsp-dump ${phspTestOutput}/phase-space-file/flipped-record.phsp)
set_tests_properties(PhspDumpCorrupted PROPERTIES
  FIXTURES_REQUIRED PhspRoundTripFile
  PASS_REGULAR_EXPRESSION "phsp-dump: phase-space file .*content digest mismatch")
add_test(NAME PhspDumpUsage COMMAND phsp-dump)
set_tests_properties(PhspDumpUsage PROPERTIES PASS_REGULAR_EXPRESSION "usage: phsp-dump")
