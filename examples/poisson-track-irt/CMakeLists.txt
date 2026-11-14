# Standalone example: inject several primary-track precursor clouds into one shared
# IRT_syn chemistry history (arrival times deterministic or Poisson).
# This project does not use any dnachem-min source or header.
cmake_minimum_required(VERSION 3.16)
project(PoissonTrackIrt LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Geant4 is needed by the executables added in later steps (capture, history, compare).
# The pure-logic library below never includes Geant4, so its tests run without a kernel.
# Geant4_DIR must point at the private patched Geant4 11.4.3 install.
find_package(Geant4 11.4.3 REQUIRED)

# ---- Geant4-free core logic -------------------------------------------------
add_library(ptirtCore STATIC
  src/PrecursorPacket.cc
  src/ArrivalSchedule.cc
  src/Ledger.cc
  src/CaptureLibrary.cc
  src/HistoryConfig.cc
  src/ActivationObserverCheck.cc
  src/SeparatedCheck.cc)
target_include_directories(ptirtCore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
if(MSVC)
  target_compile_options(ptirtCore PUBLIC /W4 /utf-8)
else()
  target_compile_options(ptirtCore PUBLIC -Wall -Wextra)
endif()

# ---- Geant4 application: capture ---------------------------------------------
# capture: shoot N primaries, write one PrecursorPacket per primary at 1 ps.
add_executable(capture
  src/CaptureMain.cc
  src/CaptureActions.cc
  src/CaptureRecorder.cc
  src/CaptureDetectorConstruction.cc
  src/ExamplePhysicsList.cc
  src/HistoryBoundedBrownianAction.cc)
target_include_directories(capture SYSTEM PRIVATE ${Geant4_INCLUDE_DIRS})
target_link_libraries(capture PRIVATE ptirtCore ${Geant4_LIBRARIES})

# capture-check: read a packet library back and validate it (Geant4-free).
add_executable(capture-check src/CaptureCheck.cc)
target_link_libraries(capture-check PRIVATE ptirtCore)

# ---- Geant4 application: history ---------------------------------------------
# history: inject the packets of an arrival schedule into ONE standalone IRT_syn
# chemistry history, each source activating at its own time.
add_executable(history
  src/HistoryMain.cc
  src/SourceInjector.cc
  src/ActivationObserver.cc
  src/HistoryDetectorConstruction.cc
  src/HistoryBoundedBrownianAction.cc
  src/ExamplePhysicsList.cc)
target_include_directories(history SYSTEM PRIVATE ${Geant4_INCLUDE_DIRS})
target_link_libraries(history PRIVATE ptirtCore ${Geant4_LIBRARIES})

# ---- compare: the separated-sources check on finished histories (Geant4-free) -
# Run by scripts/RunSeparatedCheck.ps1; the long check itself is not a ctest.
add_executable(compare src/CompareMain.cc)
target_link_libraries(compare PRIVATE ptirtCore)

# ---- Unit tests (plain checks, active even under NDEBUG) --------------------
enable_testing()
set(ptirtTestOutput ${CMAKE_CURRENT_BINARY_DIR}/test-output)
foreach(testName PrecursorPacketTest ArrivalScheduleTest LedgerTest HistoryConfigTest ActivationObserverCheckTest SeparatedCheckTest)
  add_executable(${testName} test/${testName}.cc)
  target_link_libraries(${testName} PRIVATE ptirtCore)
  add_test(NAME ${testName} COMMAND ${testName})
endforeach()

add_executable(CaptureLibraryTest test/CaptureLibraryTest.cc)
target_link_libraries(CaptureLibraryTest PRIVATE ptirtCore)
add_test(NAME CaptureLibraryTest COMMAND CaptureLibraryTest ${ptirtTestOutput}/capture-library-test)

# ---- Capture tests (run the Geant4 kernel) -----------------------------------
# Smoke: capture 2 packets with the shipped macro, read back and validate.
add_test(NAME CaptureSmoke
  COMMAND ${CMAKE_COMMAND}
    -DCAPTURE=$<TARGET_FILE:capture>
    -DCHECK=$<TARGET_FILE:capture-check>
    -DMACRO=${CMAKE_CURRENT_SOURCE_DIR}/macros/capture.mac
    -DOUTPUT_DIR=${ptirtTestOutput}/capture-smoke
    -DCOUNT=2
    -DFIRST_SEED=1001
    -P ${CMAKE_CURRENT_SOURCE_DIR}/test/RunCaptureSmoke.cmake)
# Escape: a box smaller than the track, so the ledger needs the escaped energy.
add_test(NAME CaptureEscapeLedger
  COMMAND ${CMAKE_COMMAND}
    -DCAPTURE=$<TARGET_FILE:capture>
    -DCHECK=$<TARGET_FILE:capture-check>
    -DMACRO=${CMAKE_CURRENT_SOURCE_DIR}/test/capture_escape.mac
    -DOUTPUT_DIR=${ptirtTestOutput}/capture-escape
    -DCOUNT=2
    -DFIRST_SEED=1
    -DREQUIRE_ESCAPE=ON
    -P ${CMAKE_CURRENT_SOURCE_DIR}/test/RunCaptureSmoke.cmake)
# Declared seeds: each packet is reproducible from its seed alone.
add_test(NAME CaptureSeedReproducible
  COMMAND ${CMAKE_COMMAND}
    -DCAPTURE=$<TARGET_FILE:capture>
    -DMACRO=${CMAKE_CURRENT_SOURCE_DIR}/macros/capture.mac
    -DOUTPUT_DIR=${ptirtTestOutput}/capture-seeds
    -DFIRST_SEED=1001
    -P ${CMAKE_CURRENT_SOURCE_DIR}/test/RunCaptureSeedCheck.cmake)
# Refusals: a macro selecting SBS, and invalid arguments.
add_test(NAME CaptureRejectsSbs
  COMMAND ${CMAKE_COMMAND}
    -DCAPTURE=$<TARGET_FILE:capture>
    -DOUTPUT_DIR=${ptirtTestOutput}/capture-sbs
    -DEXPECT_ERROR=IRT_syn
    "-DARGS=--macro|${CMAKE_CURRENT_SOURCE_DIR}/test/capture_sbs.mac|--output|${ptirtTestOutput}/capture-sbs|--count|1|--first-seed|1"
    -P ${CMAKE_CURRENT_SOURCE_DIR}/test/RunCaptureExpectFailure.cmake)
add_test(NAME CaptureRejectsClassicIrt
  COMMAND ${CMAKE_COMMAND}
    -DCAPTURE=$<TARGET_FILE:capture>
    -DOUTPUT_DIR=${ptirtTestOutput}/capture-irt
    -DEXPECT_ERROR=IRT_syn
    "-DARGS=--macro|${CMAKE_CURRENT_SOURCE_DIR}/test/capture_irt.mac|--output|${ptirtTestOutput}/capture-irt|--count|1|--first-seed|1"
    -P ${CMAKE_CURRENT_SOURCE_DIR}/test/RunCaptureExpectFailure.cmake)
add_test(NAME CaptureRejectsGunTime
  COMMAND ${CMAKE_COMMAND}
    -DCAPTURE=$<TARGET_FILE:capture>
    -DOUTPUT_DIR=${ptirtTestOutput}/capture-gun-time
    -DEXPECT_ERROR=gun time
    "-DARGS=--macro|${CMAKE_CURRENT_SOURCE_DIR}/test/capture_gun_time.mac|--output|${ptirtTestOutput}/capture-gun-time|--count|1|--first-seed|1"
    -P ${CMAKE_CURRENT_SOURCE_DIR}/test/RunCaptureExpectFailure.cmake)
add_test(NAME CaptureRejectsBadArguments
  COMMAND ${CMAKE_COMMAND}
    -DCAPTURE=$<TARGET_FILE:capture>
    -DOUTPUT_DIR=${ptirtTestOutput}/capture-bad-arguments
    -DEXPECT_ERROR=first-seed
    "-DARGS=--macro|${CMAKE_CURRENT_SOURCE_DIR}/macros/capture.mac|--output|${ptirtTestOutput}/capture-bad-arguments|--count|2|--first-seed|2147483647"
    -P ${CMAKE_CURRENT_SOURCE_DIR}/test/RunCaptureExpectFailure.cmake)

# ---- History tests (run the Geant4 kernel; bounded, seconds each) ------------
# Fixtures: two packet libraries captured once for all history tests.
set(ptirtHistoryLibrary ${ptirtTestOutput}/history-library)
set(ptirtHistoryBoxLibrary ${ptirtTestOutput}/history-box-library)
add_test(NAME HistoryLibrary
  COMMAND ${CMAKE_COMMAND}
    -DCAPTURE=$<TARGET_FILE:capture>
    -DCHECK=$<TARGET_FILE:capture-check>
    -DMACRO=${CMAKE_CURRENT_SOURCE_DIR}/macros/capture.mac
    -DOUTPUT_DIR=${ptirtHistoryLibrary}
    -DCOUNT=4
    -DFIRST_SEED=2001
    -P ${CMAKE_CURRENT_SOURCE_DIR}/test/RunCaptureSmoke.cmake)
set_tests_properties(HistoryLibrary PROPERTIES FIXTURES_SETUP HistoryLibrary)
# Captured in a 1 um half-side box, for the reflecting-box history.
add_test(NAME HistoryBoxLibrary
  COMMAND ${CMAKE_COMMAND}
    -DCAPTURE=$<TARGET_FILE:capture>
    -DCHECK=$<TARGET_FILE:capture-check>
    -DMACRO=${CMAKE_CURRENT_SOURCE_DIR}/test/capture_escape.mac
    -DOUTPUT_DIR=${ptirtHistoryBoxLibrary}
    -DCOUNT=2
    -DFIRST_SEED=3001
    -P ${CMAKE_CURRENT_SOURCE_DIR}/test/RunCaptureSmoke.cmake)
set_tests_properties(HistoryBoxLibrary PROPERTIES FIXTURES_SETUP HistoryBoxLibrary)

# Inputs common to the history tests below ('|' separates lines; see
# test/HistoryTestInput.cmake for @LIBRARY@ and @SCHEDULE@).
set(ptirtHistoryScript ${CMAKE_CURRENT_SOURCE_DIR}/test)
set(ptirtThreeTracks "0 0 0 0 0|50 1 0.05 0 0|100 2 60 0 0")
set(ptirtScheduleConfig "libraryDir = @LIBRARY@|scheduleFile = @SCHEDULE@")

# Deterministic 3-track smoke: two overlapping tracks (50 nm apart) and a far
# one, arriving at 0, 50 and 100 ns, in one history observed to 1 us.
add_test(NAME HistoryDeterministicSmoke
  COMMAND ${CMAKE_COMMAND}
    -DHISTORY=$<TARGET_FILE:history>
    -DWORK_DIR=${ptirtTestOutput}/history-deterministic
    -DLIBRARY=${ptirtHistoryLibrary}
    "-DCONFIG=mode = deterministic|${ptirtScheduleConfig}|endpointNs = 1000|chemistrySeed = 11"
    "-DSCHEDULE=${ptirtThreeTracks}"
    -DEXPECT_SOURCES=3 -DEXPECT_STAGES=3 -DEXPECT_LAST_TIME=1000
    -P ${ptirtHistoryScript}/RunHistorySmoke.cmake)
set_tests_properties(HistoryDeterministicSmoke PROPERTIES
  FIXTURES_REQUIRED HistoryLibrary FIXTURES_SETUP HistoryDeterministicOutput)
# Single-source control of the same schedule (row 1): same record grid.
add_test(NAME HistorySingleSmoke
  COMMAND ${CMAKE_COMMAND}
    -DHISTORY=$<TARGET_FILE:history>
    -DWORK_DIR=${ptirtTestOutput}/history-single
    -DLIBRARY=${ptirtHistoryLibrary}
    "-DCONFIG=mode = single|${ptirtScheduleConfig}|singleSource = 1|endpointNs = 1000|chemistrySeed = 12"
    "-DSCHEDULE=${ptirtThreeTracks}"
    -DEXPECT_SOURCES=1 -DEXPECT_STAGES=1 -DEXPECT_LAST_TIME=1000
    -DSAME_GRID_AS=${ptirtTestOutput}/history-deterministic/out
    -P ${ptirtHistoryScript}/RunHistorySmoke.cmake)
set_tests_properties(HistorySingleSmoke PROPERTIES
  FIXTURES_REQUIRED "HistoryLibrary;HistoryDeterministicOutput")
# Poisson smoke: 10 MHz for 200 ns in a 0.4 um box (sourceSeed 3 draws 2 arrivals).
add_test(NAME HistoryPoissonSmoke
  COMMAND ${CMAKE_COMMAND}
    -DHISTORY=$<TARGET_FILE:history>
    -DWORK_DIR=${ptirtTestOutput}/history-poisson
    -DLIBRARY=${ptirtHistoryLibrary}
    "-DCONFIG=mode = poisson|libraryDir = @LIBRARY@|rateHz = 1e7|exposureS = 2e-7|sourceSeed = 3|regionHalfSizeUm = 0.2|endpointNs = 1000|chemistrySeed = 13"
    -DEXPECT_SOURCES=2 -DEXPECT_STAGES=2 -DEXPECT_LAST_TIME=1000
    -P ${ptirtHistoryScript}/RunHistorySmoke.cmake)
set_tests_properties(HistoryPoissonSmoke PROPERTIES FIXTURES_REQUIRED HistoryLibrary)
# A Poisson draw of zero sources is a valid, unconditioned outcome: the history
# still writes zero counts on its record grid.
add_test(NAME HistoryPoissonZeroArrivals
  COMMAND ${CMAKE_COMMAND}
    -DHISTORY=$<TARGET_FILE:history>
    -DWORK_DIR=${ptirtTestOutput}/history-poisson-zero
    -DLIBRARY=${ptirtHistoryLibrary}
    "-DCONFIG=mode = poisson|libraryDir = @LIBRARY@|rateHz = 1|exposureS = 2e-7|sourceSeed = 3|regionHalfSizeUm = 0.2|endpointNs = 1000|chemistrySeed = 13"
    -DEXPECT_SOURCES=0 -DEXPECT_STAGES=0 -DEXPECT_LAST_TIME=1000
    -P ${ptirtHistoryScript}/RunHistorySmoke.cmake)
set_tests_properties(HistoryPoissonZeroArrivals PROPERTIES FIXTURES_REQUIRED HistoryLibrary)
# Reflecting box: 1.1 um half box around tracks captured in a 1 um box, to 2 us.
add_test(NAME HistoryReflectingBoxSmoke
  COMMAND ${CMAKE_COMMAND}
    -DHISTORY=$<TARGET_FILE:history>
    -DWORK_DIR=${ptirtTestOutput}/history-box
    -DLIBRARY=${ptirtHistoryBoxLibrary}
    "-DCONFIG=mode = deterministic|${ptirtScheduleConfig}|halfBoxUm = 1.1|worldHalfSideUm = 3|endpointNs = 2000|chemistrySeed = 14"
    "-DSCHEDULE=0 0 0 0 0|20 1 0.05 0 0"
    -DEXPECT_SOURCES=2 -DEXPECT_STAGES=2 -DEXPECT_LAST_TIME=2000 -DEXPECT_BOX_NM=1100
    -P ${ptirtHistoryScript}/RunHistorySmoke.cmake)
set_tests_properties(HistoryReflectingBoxSmoke PROPERTIES FIXTURES_REQUIRED HistoryBoxLibrary)

# Refusals: exit status 2, a HISTORY_ERROR matching the regex, nothing written.
# ptirt_history_refusal(<test name> <error regex> <-D inputs of HistoryTestInput.cmake>...)
set(ptirtOneTrack "0 0 0 0 0")
set(ptirtDeterministic100 "mode = deterministic|${ptirtScheduleConfig}|endpointNs = 100|chemistrySeed = 1")
function(ptirt_history_refusal name expectError)
  add_test(NAME ${name}
    COMMAND ${CMAKE_COMMAND}
      -DHISTORY=$<TARGET_FILE:history>
      -DWORK_DIR=${ptirtTestOutput}/${name}
      -DLIBRARY=${ptirtHistoryLibrary}
      "-DEXPECT_ERROR=${expectError}" -DEXPECT_STATUS=2
      ${ARGN}
      -P ${ptirtHistoryScript}/RunHistoryExpectFailure.cmake)
  set_tests_properties(${name} PROPERTIES FIXTURES_REQUIRED HistoryLibrary)
endfunction()
ptirt_history_refusal(HistoryRejectsMalformedConfig "unknown key 'colour'"
  "-DCONFIG=${ptirtDeterministic100}|colour = blue" "-DSCHEDULE=${ptirtOneTrack}")
ptirt_history_refusal(HistoryRejectsMalformedSchedule "expected 5 fields"
  "-DCONFIG=${ptirtDeterministic100}" "-DSCHEDULE=0 0 0 0")
ptirt_history_refusal(HistoryRejectsMalformedPacket "packet library .*malformed integer"
  "-DCONFIG=${ptirtDeterministic100}" "-DSCHEDULE=${ptirtOneTrack}"
  "-DPACKET_FIND=moleculeCount " "-DPACKET_REPLACE=moleculeCount x")
ptirt_history_refusal(HistoryRejectsIncompatibleTag "incompatible packet: tag"
  "-DCONFIG=${ptirtDeterministic100}" "-DSCHEDULE=${ptirtOneTrack}"
  "-DPACKET_FIND=compatibilityTag ptirt1_" "-DPACKET_REPLACE=compatibilityTag ptirt0_")
# A molecule this build does not know (renamed in the packet).
ptirt_history_refusal(HistoryRejectsIncompatiblePacket "incompatible packet: source 0 \\(packet 0\\): molecule e_aqZ"
  "-DCONFIG=${ptirtDeterministic100}" "-DSCHEDULE=${ptirtOneTrack}"
  "-DPACKET_FIND=molecule e_aq " "-DPACKET_REPLACE=molecule e_aqZ ")
ptirt_history_refusal(HistoryRejectsExhaustedLibrary "library exhausted"
  "-DCONFIG=mode = poisson|libraryDir = @LIBRARY@|rateHz = 1e9|exposureS = 1e-6|sourceSeed = 1|endpointNs = 2000|chemistrySeed = 1")
# Arrival 0.5 ps before the endpoint: activation (arrival + 1 ps) after it.
ptirt_history_refusal(HistoryRejectsActivationAfterEndpoint "activates at .*not before the endpoint"
  "-DCONFIG=${ptirtDeterministic100}" "-DSCHEDULE=99.9995 0 0 0 0")
ptirt_history_refusal(HistoryRejectsSingleSourceOutOfRange "singleSource 3 is not a row"
  "-DCONFIG=mode = single|${ptirtScheduleConfig}|singleSource = 3|endpointNs = 1000|chemistrySeed = 1"
  "-DSCHEDULE=${ptirtThreeTracks}")

set(ptirtHistoryTests HistoryLibrary HistoryBoxLibrary HistoryDeterministicSmoke HistorySingleSmoke
    HistoryPoissonSmoke HistoryPoissonZeroArrivals HistoryReflectingBoxSmoke HistoryRejectsMalformedConfig
    HistoryRejectsMalformedSchedule HistoryRejectsMalformedPacket HistoryRejectsIncompatibleTag
    HistoryRejectsIncompatiblePacket HistoryRejectsExhaustedLibrary
    HistoryRejectsActivationAfterEndpoint HistoryRejectsSingleSourceOutOfRange)

# The Geant4 DLLs of the install found through Geant4_DIR go first on PATH, so
# another Geant4 on the user's PATH is never loaded by mistake.
if(WIN32)
  get_filename_component(ptirtGeant4BinDir "${Geant4_DIR}/../../../bin" ABSOLUTE)
  set_tests_properties(CaptureSmoke CaptureEscapeLedger CaptureSeedReproducible
                       CaptureRejectsSbs CaptureRejectsClassicIrt CaptureRejectsGunTime
                       CaptureRejectsBadArguments ${ptirtHistoryTests} PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${ptirtGeant4BinDir}")
endif()
