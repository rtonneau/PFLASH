# Standalone example: dose-rate study of a 4 um water box (see README.md).
# capture writes a library of 500 keV e- box-crossing packets (precursors at
# 1 ps and the energy deposited in the box); history injects packets into one
# IRT_syn chemistry history in a reflecting box. PureWater chemistry at pH 7,
# no mesoscopic stage. Copied (not linked) from examples/poisson-track-irt and
# from the dnachem-min PureWater builder; this project uses no source or
# header of either.
cmake_minimum_required(VERSION 3.16)
project(DoseRateStudy LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Geant4_DIR must point at the private patched Geant4 11.4.3 install (source
# activation API, corrected G4ChemReboundTransportation). The pure-logic
# library below never includes Geant4, so its tests run without a kernel.
find_package(Geant4 11.4.3 REQUIRED)

# ---- Geant4-free core logic -------------------------------------------------
add_library(drsCore STATIC
  src/PrecursorPacket.cc
  src/ArrivalSchedule.cc
  src/Ledger.cc
  src/CaptureLibrary.cc
  src/HistoryConfig.cc
  src/ActivationObserverCheck.cc
  src/TimeGrid.cc
  src/SteadyStateDetector.cc)
target_include_directories(drsCore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
if(MSVC)
  target_compile_options(drsCore PUBLIC /W4 /utf-8)
else()
  target_compile_options(drsCore PUBLIC -Wall -Wextra)
endif()

# Physics and chemistry shared by capture and history.
set(drsPhysicsSources
  src/DrsPhysicsList.cc
  src/PureWaterChemistryList.cc
  src/PureWaterReactions.cc
  src/HistoryBoundedBrownianAction.cc)

# ---- Geant4 application: capture ---------------------------------------------
# capture: shoot N primaries across the capture box, write one PrecursorPacket
# per primary (molecules inside the box at 1 ps, energy deposited in the box).
add_executable(capture
  src/CaptureMain.cc
  src/CaptureActions.cc
  src/CaptureRecorder.cc
  src/CaptureDetectorConstruction.cc
  ${drsPhysicsSources})
target_include_directories(capture SYSTEM PRIVATE ${Geant4_INCLUDE_DIRS})
target_link_libraries(capture PRIVATE drsCore ${Geant4_LIBRARIES})

# capture-check: read a packet library back and validate it (Geant4-free).
add_executable(capture-check src/CaptureCheck.cc)
target_link_libraries(capture-check PRIVATE drsCore)

# ---- Geant4 application: history ---------------------------------------------
# history: inject the packets of an arrival schedule into ONE IRT_syn chemistry
# history, each source activating at its own time.
add_executable(history
  src/HistoryMain.cc
  src/SourceInjector.cc
  src/ActivationObserver.cc
  src/HistoryDetectorConstruction.cc
  ${drsPhysicsSources})
target_include_directories(history SYSTEM PRIVATE ${Geant4_INCLUDE_DIRS})
target_link_libraries(history PRIVATE drsCore ${Geant4_LIBRARIES})

# ---- Unit tests (plain checks, active even under NDEBUG) --------------------
enable_testing()
set(drsTestOutput ${CMAKE_CURRENT_BINARY_DIR}/test-output)
foreach(testName PrecursorPacketTest ArrivalScheduleTest LedgerTest HistoryConfigTest ActivationObserverCheckTest
                 TimeGridTest SteadyStateDetectorTest)
  add_executable(${testName} test/${testName}.cc)
  target_link_libraries(${testName} PRIVATE drsCore)
  add_test(NAME ${testName} COMMAND ${testName})
endforeach()

add_executable(CaptureLibraryTest test/CaptureLibraryTest.cc)
target_link_libraries(CaptureLibraryTest PRIVATE drsCore)
add_test(NAME CaptureLibraryTest COMMAND CaptureLibraryTest ${drsTestOutput}/capture-library-test)

# ---- Capture smoke (runs the Geant4 kernel) ----------------------------------
# 20 packets with the shipped macro (500 keV e-, 2 um half-side box in a 10 um
# half-side world): count, every molecule inside the box, mean box deposit > 0
# (reported). A primary launched near the edge of the box face can scatter out
# of it upstream and miss the box (no molecule, no deposit); capture skips such
# events (up to MAX_EMPTY) and lists them in capture.json. The library is also
# the fixture of the history smoke.
set(drsCaptureLibrary ${drsTestOutput}/capture-smoke)
add_test(NAME CaptureSmoke
  COMMAND ${CMAKE_COMMAND}
    -DCAPTURE=$<TARGET_FILE:capture>
    -DCHECK=$<TARGET_FILE:capture-check>
    -DMACRO=${CMAKE_CURRENT_SOURCE_DIR}/macros/capture.mac
    -DOUTPUT_DIR=${drsCaptureLibrary}
    -DCOUNT=20
    -DFIRST_SEED=1001
    -DBOX_HALF_SIDE_NM=2000
    -DMAX_EMPTY=20
    -P ${CMAKE_CURRENT_SOURCE_DIR}/test/RunCaptureSmoke.cmake)
set_tests_properties(CaptureSmoke PROPERTIES FIXTURES_SETUP DrsCaptureLibrary)

# ---- History smoke (skeleton: load the library and run) ----------------------
# Two packets at 0 and 20 ns, unshifted, in the 2 um reflecting half box, to 100 ns.
add_test(NAME HistorySmoke
  COMMAND ${CMAKE_COMMAND}
    -DHISTORY=$<TARGET_FILE:history>
    -DWORK_DIR=${drsTestOutput}/history-smoke
    -DLIBRARY=${drsCaptureLibrary}
    "-DCONFIG=mode = deterministic|libraryDir = @LIBRARY@|scheduleFile = @SCHEDULE@|halfBoxUm = 2|worldHalfSideUm = 3|endpointNs = 100|chemistrySeed = 14"
    "-DSCHEDULE=0 0 0 0 0|20 1 0 0 0"
    -DEXPECT_SOURCES=2 -DEXPECT_STAGES=2 -DEXPECT_LAST_TIME=100 -DEXPECT_BOX_NM=2000
    -P ${CMAKE_CURRENT_SOURCE_DIR}/test/RunHistorySmoke.cmake)
set_tests_properties(HistorySmoke PROPERTIES FIXTURES_REQUIRED DrsCaptureLibrary)

# ---- History dose cutoff (1e4 Gy/s) -----------------------------------------
# The 20-packet fixture library holds ~0.037 Gy of the 4 um box (1 Gy = 3.99e5
# eV, ~600 tracks), so these tests stop at a small dose. HistoryDoseCutoff:
# ~3 arrivals to 0.005 Gy, no steady-state test; the CSV, the ledger and the
# activation and energy checks. HistoryDoseSteadyState: ~11 arrivals to 0.02 Gy
# with 2-sample windows and a tolerance loose enough to stop early, so the
# later rows are extrapolated.
set(drsDoseConfig
  "mode = doseCutoff|libraryDir = @LIBRARY@|halfBoxUm = 2|worldHalfSideUm = 3|chemistrySeed = 21|sourceSeed = 7|doseRateGyPerS = 1e4")
add_test(NAME HistoryDoseCutoff
  COMMAND ${CMAKE_COMMAND}
    -DHISTORY=$<TARGET_FILE:history>
    -DWORK_DIR=${drsTestOutput}/history-dose-cutoff
    -DLIBRARY=${drsCaptureLibrary}
    "-DCONFIG=${drsDoseConfig}|doseGy = 0.005|steadyStateWindow = 0"
    -P ${CMAKE_CURRENT_SOURCE_DIR}/test/RunHistoryDose.cmake)
add_test(NAME HistoryDoseSteadyState
  COMMAND ${CMAKE_COMMAND}
    -DHISTORY=$<TARGET_FILE:history>
    -DWORK_DIR=${drsTestOutput}/history-dose-steady-state
    -DLIBRARY=${drsCaptureLibrary}
    "-DCONFIG=${drsDoseConfig}|doseGy = 0.02|steadyStateWindow = 2|steadyStateTolerance = 1000|steadyStateMinMean = 5"
    -DEXPECT_STEADY_STATE=ON
    -P ${CMAKE_CURRENT_SOURCE_DIR}/test/RunHistoryDose.cmake)
set_tests_properties(HistoryDoseCutoff HistoryDoseSteadyState PROPERTIES FIXTURES_REQUIRED DrsCaptureLibrary)

# The Geant4 DLLs of the install found through Geant4_DIR go first on PATH, so
# another Geant4 on the user's PATH is never loaded by mistake.
if(WIN32)
  get_filename_component(drsGeant4BinDir "${Geant4_DIR}/../../../bin" ABSOLUTE)
  set_tests_properties(CaptureSmoke HistorySmoke HistoryDoseCutoff HistoryDoseSteadyState PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${drsGeant4BinDir}")
endif()
