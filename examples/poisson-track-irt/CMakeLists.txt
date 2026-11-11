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
# Geant4_DIR points at a Geant4 11.4.3 install: stock by default, or the private
# patched install when PTIRT_PATCHED_GEANT4 is ON.
find_package(Geant4 11.4.3 REQUIRED)

# ---- Build mode: stock or patched Geant4 ------------------------------------
# The patched build uses G4DNAIndependentReactionTimeStepper::ResetSourceActivations
# and RegisterSourceActivation (patches/geant4/11.4.3/timed-injection/). The option
# must agree with the install: the headers are compiled against, so a mismatch
# stops the configure instead of giving a binary that lies about its build mode.
option(PTIRT_PATCHED_GEANT4 "Build against a Geant4 11.4.3 with the timed-injection patches" OFF)

include(CheckCXXSourceCompiles)
# Compile only (no link): the check needs the declarations, not the Geant4 libraries.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
set(CMAKE_REQUIRED_INCLUDES ${Geant4_INCLUDE_DIRS})
set(CMAKE_REQUIRED_DEFINITIONS ${Geant4_DEFINITIONS})
# Not cached across configures: Geant4_DIR or the install may have changed.
unset(PTIRT_GEANT4_HEADERS_COMPILE CACHE)
unset(PTIRT_GEANT4_HAS_SOURCE_PATCH CACHE)
check_cxx_source_compiles("
#include \"G4DNAIndependentReactionTimeStepper.hh\"
#include \"globals.hh\"
int main() { return 0; }
" PTIRT_GEANT4_HEADERS_COMPILE)
if(NOT PTIRT_GEANT4_HEADERS_COMPILE)
  message(FATAL_ERROR
    "Cannot compile against the Geant4 headers of ${Geant4_DIR} "
    "(G4DNAIndependentReactionTimeStepper.hh); see CMakeFiles/CMakeError.log. "
    "This is not a build-mode mismatch.")
endif()
check_cxx_source_compiles("
#include \"G4DNAIndependentReactionTimeStepper.hh\"
#include \"globals.hh\"
int main()
{
  void (*reset)() = &G4DNAIndependentReactionTimeStepper::ResetSourceActivations;
  void (*registerSource)(G4int, G4double) = &G4DNAIndependentReactionTimeStepper::RegisterSourceActivation;
  return reset == nullptr || registerSource == nullptr;
}
" PTIRT_GEANT4_HAS_SOURCE_PATCH)
unset(CMAKE_REQUIRED_INCLUDES)
unset(CMAKE_REQUIRED_DEFINITIONS)
unset(CMAKE_TRY_COMPILE_TARGET_TYPE)

if(PTIRT_PATCHED_GEANT4 AND NOT PTIRT_GEANT4_HAS_SOURCE_PATCH)
  message(FATAL_ERROR
    "PTIRT_PATCHED_GEANT4=ON but ${Geant4_DIR} has no source-activation API "
    "(G4DNAIndependentReactionTimeStepper::ResetSourceActivations and "
    "RegisterSourceActivation): this is a stock Geant4. Point Geant4_DIR at the patched "
    "install, or configure with -DPTIRT_PATCHED_GEANT4=OFF. "
    "See patches/geant4/11.4.3/timed-injection/README.md.")
endif()
if(NOT PTIRT_PATCHED_GEANT4 AND PTIRT_GEANT4_HAS_SOURCE_PATCH)
  message(FATAL_ERROR
    "PTIRT_PATCHED_GEANT4=OFF but ${Geant4_DIR} is patched (it has "
    "G4DNAIndependentReactionTimeStepper::ResetSourceActivations). Configure with "
    "-DPTIRT_PATCHED_GEANT4=ON for this install, or point Geant4_DIR at a stock 11.4.3.")
endif()

# The stock build's history zeroes every reaction probability while the chemistry
# runs (include/StartTrialGate.hh), to remove the start-time contact trial the
# first packet would get a second time. That is only safe while GetProbability()
# has no IRT_syn reader other than this trial
# (G4DNAIndependentReactionTimeStepper.cc:170 in 11.4.3), so another minor
# version must be audited again before it is allowed here. Only 11.4.3 was
# audited; later 11.4 patch releases are assumed not to change the stepper or
# the reaction data (residual risk, recorded in ADR 0010).
if(NOT PTIRT_PATCHED_GEANT4
   AND (Geant4_VERSION VERSION_LESS 11.4 OR NOT Geant4_VERSION VERSION_LESS 11.5))
  message(FATAL_ERROR
    "The stock build needs Geant4 11.4.x, found ${Geant4_VERSION} (${Geant4_DIR}). "
    "Its start-trial gate (include/StartTrialGate.hh) relies on GetProbability() having "
    "no IRT_syn reader other than the start-time contact trial "
    "(G4DNAIndependentReactionTimeStepper.cc:170 in 11.4.3); re-audit the readers in "
    "source/processes/electromagnetic/dna before allowing another version.")
endif()

if(PTIRT_PATCHED_GEANT4)
  set(ptirtPatchedValue 1)
  message(STATUS "poisson-track-irt build mode: patched Geant4 (${Geant4_DIR})")
else()
  set(ptirtPatchedValue 0)
  message(STATUS "poisson-track-irt build mode: stock Geant4 (${Geant4_DIR})")
endif()

# ---- Geant4-free core logic -------------------------------------------------
add_library(ptirtCore STATIC
  src/PrecursorPacket.cc
  src/PacketDigest.cc
  src/ArrivalSchedule.cc
  src/Ledger.cc
  src/CaptureLibrary.cc
  src/HistoryConfig.cc
  src/ActivationObserverCheck.cc
  src/SeparatedCheck.cc
  src/LoadedModules.cc
  src/ReflectingFold.cc
  src/BoxGuard.cc
  src/CoincidenceCheck.cc
  src/ContactCount.cc)
target_include_directories(ptirtCore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
# include/BuildMode.hh turns this into constants; it has no default.
target_compile_definitions(ptirtCore PUBLIC PTIRT_PATCHED_GEANT4=${ptirtPatchedValue})
if(WIN32)
  target_link_libraries(ptirtCore PUBLIC psapi)  # LoadedModules.cc: EnumProcessModules
endif()
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
  src/StartTrialGate.cc
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
foreach(testName PrecursorPacketTest PacketDigestTest ArrivalScheduleTest LedgerTest HistoryConfigTest ActivationObserverCheckTest SeparatedCheckTest BuildModeTest ReflectingFoldTest BoxGuardTest CoincidenceCheckTest ContactCountTest)
  add_executable(${testName} test/${testName}.cc)
  target_link_libraries(${testName} PRIVATE ptirtCore)
  add_test(NAME ${testName} COMMAND ${testName})
endforeach()

add_executable(CaptureLibraryTest test/CaptureLibraryTest.cc)
target_link_libraries(CaptureLibraryTest PRIVATE ptirtCore)
add_test(NAME CaptureLibraryTest COMMAND CaptureLibraryTest ${ptirtTestOutput}/capture-library-test)

# StartTrialGate on real G4DNAMolecularReactionData objects: Geant4 classes, no kernel.
add_executable(StartTrialGateTest test/StartTrialGateTest.cc src/StartTrialGate.cc)
target_include_directories(StartTrialGateTest SYSTEM PRIVATE ${Geant4_INCLUDE_DIRS})
target_link_libraries(StartTrialGateTest PRIVATE ptirtCore ${Geant4_LIBRARIES})
add_test(NAME StartTrialGateTest COMMAND StartTrialGateTest)

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
# Capture stage: where the packet is taken, run twice, and the capture.json fields
# that describe it (test/RunCaptureStageCheck.cmake). The stock build captures
# after the start-up contacts, the patched build at chemistry start.
if(PTIRT_PATCHED_GEANT4)
  set(ptirtStageTest CaptureAtChemistryStart)
  set(ptirtExpectBuild patched)
  set(ptirtExpectStage chemistryStart)
else()
  set(ptirtStageTest CaptureAfterStartupContacts)
  set(ptirtExpectBuild stock)
  set(ptirtExpectStage afterStartupContacts)
endif()
if(WIN32)
  set(ptirtExpectModules ON)  # the loaded modules can be listed on Windows
else()
  set(ptirtExpectModules OFF)
endif()
add_test(NAME ${ptirtStageTest}
  COMMAND ${CMAKE_COMMAND}
    -DCAPTURE=$<TARGET_FILE:capture>
    -DMACRO=${CMAKE_CURRENT_SOURCE_DIR}/macros/capture.mac
    -DOUTPUT_DIR=${ptirtTestOutput}/capture-stage
    -DFIRST_SEED=1001
    -DCOUNT=4
    -DEXPECT_BUILD=${ptirtExpectBuild}
    -DEXPECT_STAGE=${ptirtExpectStage}
    -DEXPECT_MODULES=${ptirtExpectModules}
    -P ${CMAKE_CURRENT_SOURCE_DIR}/test/RunCaptureStageCheck.cmake)
set(ptirtStageTests ${ptirtStageTest})
if(NOT PTIRT_PATCHED_GEANT4)
  # The stock build derives the chemistry seed from the physics seed (2147483648 - S),
  # so physics seeds stop at 1073741823: that one is captured, the next is refused.
  add_test(NAME CaptureAcceptsStockSeedCeiling
    COMMAND ${CMAKE_COMMAND}
      -DCAPTURE=$<TARGET_FILE:capture>
      -DCHECK=$<TARGET_FILE:capture-check>
      -DMACRO=${CMAKE_CURRENT_SOURCE_DIR}/macros/capture.mac
      -DOUTPUT_DIR=${ptirtTestOutput}/capture-seed-ceiling
      -DCOUNT=1
      -DFIRST_SEED=1073741823
      -P ${CMAKE_CURRENT_SOURCE_DIR}/test/RunCaptureSmoke.cmake)
  add_test(NAME CaptureRejectsStockSeedAboveCeiling
    COMMAND ${CMAKE_COMMAND}
      -DCAPTURE=$<TARGET_FILE:capture>
      -DOUTPUT_DIR=${ptirtTestOutput}/capture-seed-above-ceiling
      -DEXPECT_ERROR=first-seed.*1073741823
      "-DARGS=--macro|${CMAKE_CURRENT_SOURCE_DIR}/macros/capture.mac|--output|${ptirtTestOutput}/capture-seed-above-ceiling|--count|1|--first-seed|1073741824"
      -P ${CMAKE_CURRENT_SOURCE_DIR}/test/RunCaptureExpectFailure.cmake)
  list(APPEND ptirtStageTests CaptureAcceptsStockSeedCeiling CaptureRejectsStockSeedAboveCeiling)
endif()
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

# Ledger fields every history smoke requires (test/RunHistorySmoke.cmake).
set(ptirtLedgerExpect -DEXPECT_BUILD=${ptirtExpectBuild} -DEXPECT_MODULES=${ptirtExpectModules})

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
    ${ptirtLedgerExpect}
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
    ${ptirtLedgerExpect}
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
    ${ptirtLedgerExpect}
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
    ${ptirtLedgerExpect}
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
    -DEXPECT_BOX_GUARD=ok ${ptirtLedgerExpect}
    -P ${ptirtHistoryScript}/RunHistorySmoke.cmake)
set_tests_properties(HistoryReflectingBoxSmoke PROPERTIES FIXTURES_REQUIRED HistoryBoxLibrary)

# New-old contact limit: the one failure that still writes its outputs (exit 1,
# PTIRT_NEW_OLD_CONTACTS, ledger checks.allPassed false). Source 1, a copy of source 0,
# arrives 1 ns after it, 5 nm away, so some of its fresh molecules sit inside the
# reaction radius of the old ones (10 pairs stock, 3 patched; at 10 ns the count was
# 0 or 1, by chance). A second run with the limit at the observed count must pass.
add_test(NAME HistoryNewOldContactLimit
  COMMAND ${CMAKE_COMMAND}
    -DHISTORY=$<TARGET_FILE:history>
    -DWORK_DIR=${ptirtTestOutput}/history-contact-limit
    -DLIBRARY=${ptirtHistoryLibrary}
    "-DCONFIG=mode = deterministic|${ptirtScheduleConfig}|endpointNs = 1000|chemistrySeed = 21|newOldContactLimit = 0"
    "-DSCHEDULE=0 0 0 0 0|1 1 0.005 0 0" -DPACKET_DUPLICATE=ON
    -P ${ptirtHistoryScript}/RunHistoryContactLimit.cmake)
set_tests_properties(HistoryNewOldContactLimit PROPERTIES FIXTURES_REQUIRED HistoryLibrary)

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
  "-DPACKET_FIND=compatibilityTag ptirt2_" "-DPACKET_REPLACE=compatibilityTag ptirt1_")
# A molecule this build does not know (renamed in the packet).
ptirt_history_refusal(HistoryRejectsIncompatiblePacket "incompatible packet: source 0 \\(packet 0\\): molecule e_aqZ"
  "-DCONFIG=${ptirtDeterministic100}" "-DSCHEDULE=${ptirtOneTrack}"
  "-DPACKET_FIND=molecule e_aq " "-DPACKET_REPLACE=molecule e_aqZ ")
# Two molecules of one packet at one position (a hand-made packet): refused with the token.
ptirt_history_refusal(HistoryRejectsCoincidentPacket "PTIRT_COINCIDENT_MOLECULES count=2 positions=1"
  "-DCONFIG=${ptirtDeterministic100}" "-DSCHEDULE=${ptirtOneTrack}" -DPACKET_COINCIDE=ON)
# Two sources at one offset whose packets hold the same molecules (packet 1 is a copy
# of packet 0): every molecule meets its copy, across sources.
ptirt_history_refusal(HistoryRejectsCoincidentSources "PTIRT_COINCIDENT_MOLECULES count=[0-9]+ positions=[0-9]+"
  "-DCONFIG=${ptirtDeterministic100}" "-DSCHEDULE=0 0 0 0 0|50 1 0 0 0" -DPACKET_DUPLICATE=ON)
ptirt_history_refusal(HistoryRejectsExhaustedLibrary "library exhausted"
  "-DCONFIG=mode = poisson|libraryDir = @LIBRARY@|rateHz = 1e9|exposureS = 1e-6|sourceSeed = 1|endpointNs = 2000|chemistrySeed = 1")
# Arrival 0.5 ps before the endpoint: activation (arrival + 1 ps) after it.
ptirt_history_refusal(HistoryRejectsActivationAfterEndpoint "activates at .*not before the endpoint"
  "-DCONFIG=${ptirtDeterministic100}" "-DSCHEDULE=99.9995 0 0 0 0")
ptirt_history_refusal(HistoryRejectsSingleSourceOutOfRange "singleSource 3 is not a row"
  "-DCONFIG=mode = single|${ptirtScheduleConfig}|singleSource = 3|endpointNs = 1000|chemistrySeed = 1"
  "-DSCHEDULE=${ptirtThreeTracks}")

# Stock only: the start-trial gate (include/StartTrialGate.hh). Packet 0 of the
# HistoryLibrary fixture alone at the start time, to 10 ns, run twice: no reaction
# in a zero time step at the start time, the same species.csv on repeat.
if(NOT PTIRT_PATCHED_GEANT4)
  add_test(NAME HistorySmokeStartGate
    COMMAND ${CMAKE_COMMAND}
      -DHISTORY=$<TARGET_FILE:history>
      -DWORK_DIR=${ptirtTestOutput}/history-start-gate
      -DLIBRARY=${ptirtHistoryLibrary}
      "-DCONFIG=mode = deterministic|${ptirtScheduleConfig}|endpointNs = 10|chemistrySeed = 5001"
      "-DSCHEDULE=0 0 0 0 0"
      -P ${ptirtHistoryScript}/RunHistoryStartGate.cmake)
  set_tests_properties(HistorySmokeStartGate PROPERTIES FIXTURES_REQUIRED HistoryLibrary)
  set(ptirtStockHistoryTests HistorySmokeStartGate)
endif()

set(ptirtHistoryTests HistoryLibrary HistoryBoxLibrary HistoryDeterministicSmoke HistorySingleSmoke
    HistoryPoissonSmoke HistoryPoissonZeroArrivals HistoryReflectingBoxSmoke HistoryNewOldContactLimit
    ${ptirtStockHistoryTests}
    HistoryRejectsMalformedConfig
    HistoryRejectsMalformedSchedule HistoryRejectsMalformedPacket HistoryRejectsIncompatibleTag
    HistoryRejectsIncompatiblePacket HistoryRejectsCoincidentPacket HistoryRejectsCoincidentSources
    HistoryRejectsExhaustedLibrary
    HistoryRejectsActivationAfterEndpoint HistoryRejectsSingleSourceOutOfRange)

# The Geant4 DLLs of the install found through Geant4_DIR go first on PATH, so
# another Geant4 on the user's PATH is never loaded by mistake.
if(WIN32)
  get_filename_component(ptirtGeant4BinDir "${Geant4_DIR}/../../../bin" ABSOLUTE)
  set_tests_properties(CaptureSmoke CaptureEscapeLedger CaptureSeedReproducible ${ptirtStageTests}
                       CaptureRejectsSbs CaptureRejectsClassicIrt CaptureRejectsGunTime
                       CaptureRejectsBadArguments ${ptirtHistoryTests} StartTrialGateTest PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${ptirtGeant4BinDir}")
endif()
