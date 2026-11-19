# Standalone example: dose-rate study of a 4 um water box (see README.md).
# capture writes a library of 500 keV e- box-crossing packets (precursors at
# 1 ps and the energy deposited in the box); history injects packets into one
# IRT_syn chemistry history in a reflecting box. PureWater chemistry at pH 7,
# no mesoscopic stage. Copied (not linked) from examples/poisson-track-irt and
# from the PFLASH PureWater builder; this project uses no source or
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

# ---- Source activation API (source-contacts-experimental.patch) -------------
# Detected at configure time: without it the history still builds and runs,
# injecting sources as plain delayed tracks (ledger.json sourceActivation false).
include(CheckCXXSourceCompiles)
set(CMAKE_REQUIRED_INCLUDES ${Geant4_INCLUDE_DIRS})
set(CMAKE_REQUIRED_LIBRARIES ${Geant4_LIBRARIES})
set(CMAKE_REQUIRED_DEFINITIONS ${Geant4_DEFINITIONS})
check_cxx_source_compiles([=[
#include "G4DNAIndependentReactionTimeStepper.hh"
int main() { G4DNAIndependentReactionTimeStepper::RegisterSourceActivation(1, 0.); return 0; }
]=] DRS_SOURCE_ACTIVATION_FOUND)
unset(CMAKE_REQUIRED_INCLUDES)
unset(CMAKE_REQUIRED_LIBRARIES)
unset(CMAKE_REQUIRED_DEFINITIONS)
if(DRS_SOURCE_ACTIVATION_FOUND)
  set(DRS_HAS_SOURCE_ACTIVATION 1)
else()
  set(DRS_HAS_SOURCE_ACTIVATION 0)
endif()
message(STATUS "Source activation API (RegisterSourceActivation): ${DRS_HAS_SOURCE_ACTIVATION}")

# ---- HDF5 (optional: the particle snapshots, snapshots.h5) ------------------
# AUTO uses HDF5 when found, ON requires it, OFF does not look for it. Without
# it SnapshotWriterNoHdf5.cc is built and `snapshots = true` is a config error.
set(DRS_WITH_HDF5 "AUTO" CACHE STRING "HDF5 support for snapshots.h5: AUTO, ON or OFF")
set_property(CACHE DRS_WITH_HDF5 PROPERTY STRINGS AUTO ON OFF)
string(TOUPPER "${DRS_WITH_HDF5}" drsWithHdf5)
set(DRS_HAS_HDF5 FALSE)
if(drsWithHdf5 STREQUAL "ON")
  find_package(HDF5 REQUIRED COMPONENTS C CXX)
elseif(drsWithHdf5 STREQUAL "AUTO")
  find_package(HDF5 COMPONENTS C CXX)
elseif(NOT drsWithHdf5 STREQUAL "OFF")
  message(FATAL_ERROR "DRS_WITH_HDF5 must be AUTO, ON or OFF (got '${DRS_WITH_HDF5}')")
endif()
add_library(drsHdf5 INTERFACE)
if(NOT drsWithHdf5 STREQUAL "OFF" AND HDF5_FOUND)
  # One link target for C + C++, whatever form find_package(HDF5) produced:
  # the HDF5 package config (vcpkg), FindHDF5 targets, or FindHDF5 variables.
  if(TARGET hdf5::hdf5-shared AND TARGET hdf5::hdf5_cpp-shared)
    target_link_libraries(drsHdf5 INTERFACE hdf5::hdf5_cpp-shared hdf5::hdf5-shared)
    set(DRS_HAS_HDF5 TRUE)
  elseif(TARGET hdf5::hdf5 AND TARGET hdf5::hdf5_cpp)
    target_link_libraries(drsHdf5 INTERFACE hdf5::hdf5_cpp hdf5::hdf5)
    set(DRS_HAS_HDF5 TRUE)
  elseif(HDF5_LIBRARIES AND HDF5_INCLUDE_DIRS)
    target_link_libraries(drsHdf5 INTERFACE ${HDF5_LIBRARIES})
    target_include_directories(drsHdf5 INTERFACE ${HDF5_INCLUDE_DIRS})
    set(DRS_HAS_HDF5 TRUE)
  elseif(drsWithHdf5 STREQUAL "ON")
    message(FATAL_ERROR "HDF5 was found but provides no usable C + C++ link target")
  endif()
endif()
if(DRS_HAS_HDF5)
  message(STATUS "HDF5: snapshots.h5 available")
else()
  message(STATUS "HDF5 not used: snapshots.h5 unavailable (snapshots = true is a config error)")
endif()

# ---- Geant4-free core logic -------------------------------------------------
add_library(drsCore STATIC
  src/PrecursorPacket.cc
  src/ArrivalSchedule.cc
  src/Ledger.cc
  src/ReactionLog.cc
  src/CaptureLibrary.cc
  src/HistoryConfig.cc
  src/ActivationObserverCheck.cc
  src/TimeGrid.cc
  src/SteadyStateDetector.cc)
if(DRS_HAS_HDF5)
  target_sources(drsCore PRIVATE src/SnapshotWriter.cc)
  target_link_libraries(drsCore PUBLIC drsHdf5)
else()
  target_sources(drsCore PRIVATE src/SnapshotWriterNoHdf5.cc)
endif()
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

# snapshots-check: validate the snapshots.h5 of a doseCutoff history (HDF5 only).
if(DRS_HAS_HDF5)
  add_executable(snapshots-check src/SnapshotCheck.cc)
  target_link_libraries(snapshots-check PRIVATE drsCore)
endif()

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
target_compile_definitions(history PRIVATE DRS_HAS_SOURCE_ACTIVATION=${DRS_HAS_SOURCE_ACTIVATION})

# ---- Unit tests (plain checks, active even under NDEBUG) --------------------
enable_testing()
set(drsTestOutput ${CMAKE_CURRENT_BINARY_DIR}/test-output)
foreach(testName PrecursorPacketTest ArrivalScheduleTest LedgerTest HistoryConfigTest ActivationObserverCheckTest
                 TimeGridTest SteadyStateDetectorTest ReactionLogTest)
  add_executable(${testName} test/${testName}.cc)
  target_link_libraries(${testName} PRIVATE drsCore)
  add_test(NAME ${testName} COMMAND ${testName})
endforeach()

add_executable(SnapshotWriterTest test/SnapshotWriterTest.cc)
target_link_libraries(SnapshotWriterTest PRIVATE drsCore)
add_test(NAME SnapshotWriterTest COMMAND SnapshotWriterTest ${drsTestOutput}/snapshot-writer-test)

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

# ---- History snapshots (1e4 Gy/s, 0.005 Gy) -----------------------------------
# The HistoryDoseCutoff history with `snapshots = true`: snapshots.h5 is
# written and checked (snapshots-check: frames, species counts against
# species.csv, positions in the box); the same history without the key writes
# no HDF5 file. Without HDF5 the key is a config error (exit status 2).
if(DRS_HAS_HDF5)
  set(drsSnapshotsCheck $<TARGET_FILE:snapshots-check>)
else()
  set(drsSnapshotsCheck "")
endif()
add_test(NAME HistorySnapshots
  COMMAND ${CMAKE_COMMAND}
    -DHISTORY=$<TARGET_FILE:history>
    -DSNAPSHOTS_CHECK=${drsSnapshotsCheck}
    -DWORK_DIR=${drsTestOutput}/history-snapshots
    -DLIBRARY=${drsCaptureLibrary}
    "-DCONFIG=${drsDoseConfig}|doseGy = 0.005|steadyStateWindow = 0"
    -DBOX_HALF_SIDE_NM=2000
    -P ${CMAKE_CURRENT_SOURCE_DIR}/test/RunHistorySnapshots.cmake)
set_tests_properties(HistorySnapshots PROPERTIES FIXTURES_REQUIRED DrsCaptureLibrary)

# The Geant4 DLLs of the install found through Geant4_DIR go first on PATH, so
# another Geant4 on the user's PATH is never loaded by mistake.
if(WIN32)
  get_filename_component(drsGeant4BinDir "${Geant4_DIR}/../../../bin" ABSOLUTE)
  set_tests_properties(CaptureSmoke HistorySmoke HistoryDoseCutoff HistoryDoseSteadyState HistorySnapshots PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${drsGeant4BinDir}")
endif()
