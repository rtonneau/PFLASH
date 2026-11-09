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
  src/Ledger.cc)
target_include_directories(ptirtCore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
if(MSVC)
  target_compile_options(ptirtCore PUBLIC /W4 /utf-8)
else()
  target_compile_options(ptirtCore PUBLIC -Wall -Wextra)
endif()

# ---- Unit tests (plain checks, active even under NDEBUG) --------------------
enable_testing()
foreach(testName PrecursorPacketTest ArrivalScheduleTest LedgerTest)
  add_executable(${testName} test/${testName}.cc)
  target_link_libraries(${testName} PRIVATE ptirtCore)
  add_test(NAME ${testName} COMMAND ${testName})
endforeach()
