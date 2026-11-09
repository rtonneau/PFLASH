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
  src/CaptureLibrary.cc)
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
  src/ExamplePhysicsList.cc)
target_include_directories(capture SYSTEM PRIVATE ${Geant4_INCLUDE_DIRS})
target_link_libraries(capture PRIVATE ptirtCore ${Geant4_LIBRARIES})

# capture-check: read a packet library back and validate it (Geant4-free).
add_executable(capture-check src/CaptureCheck.cc)
target_link_libraries(capture-check PRIVATE ptirtCore)

# ---- Unit tests (plain checks, active even under NDEBUG) --------------------
enable_testing()
set(ptirtTestOutput ${CMAKE_CURRENT_BINARY_DIR}/test-output)
foreach(testName PrecursorPacketTest ArrivalScheduleTest LedgerTest)
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

# The Geant4 DLLs of the install found through Geant4_DIR go first on PATH, so
# another Geant4 on the user's PATH is never loaded by mistake.
if(WIN32)
  get_filename_component(ptirtGeant4BinDir "${Geant4_DIR}/../../../bin" ABSOLUTE)
  set_tests_properties(CaptureSmoke CaptureEscapeLedger CaptureSeedReproducible
                       CaptureRejectsSbs CaptureRejectsClassicIrt CaptureRejectsGunTime
                       CaptureRejectsBadArguments PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${ptirtGeant4BinDir}")
endif()
