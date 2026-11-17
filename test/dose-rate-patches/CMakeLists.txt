# Patch-necessity suite: sufficiency fixtures, one per dose-rate Geant4 patch.
# Configure once per kernel variant, into its own build dir:
#   cmake -S test/dose-rate-patches -B <dir> -G Ninja -DCMAKE_BUILD_TYPE=Release
#         -DGeant4_DIR=<kernel install>/lib/cmake/Geant4 -DDRP_KERNEL_VARIANT=<stock|p1|...>
#         -DCMAKE_TOOLCHAIN_FILE=<vcpkg toolchain>
# Each test runs its fixture with the expectation implied by the variant:
# 'fixed' when the variant carries the fixture's patch, 'defect' otherwise.
# Release only: the kernels are Release builds (Debug STL layouts differ).
cmake_minimum_required(VERSION 3.22)
project(DoseRatePatchNecessity LANGUAGES CXX)
enable_testing()
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Geant4 11.4.3 EXACT REQUIRED)
include(${Geant4_USE_FILE})

set(DRP_KERNEL_VARIANT "" CACHE STRING "Kernel variant this build runs on (stock, p1, p2, p3, all, all-1, all-2, all-3)")
set(variantPatches_stock "")
set(variantPatches_p1 1)
set(variantPatches_p2 2)
set(variantPatches_p3 3)
set(variantPatches_all 1 2 3)
set(variantPatches_all-1 2 3)
set(variantPatches_all-2 1 3)
set(variantPatches_all-3 1 2)
if(NOT DEFINED variantPatches_${DRP_KERNEL_VARIANT} OR DRP_KERNEL_VARIANT STREQUAL "")
  message(FATAL_ERROR "DRP_KERNEL_VARIANT must be one of stock, p1, p2, p3, all, all-1, all-2, all-3")
endif()
set(kernelPatches ${variantPatches_${DRP_KERNEL_VARIANT}})

# The kernel's DLLs must come first on PATH at run time.
get_filename_component(kernelBin "${Geant4_DIR}/../../../bin" ABSOLUTE)
set(runtimePath "${kernelBin}")
if(DEFINED VCPKG_INSTALLED_DIR AND DEFINED VCPKG_TARGET_TRIPLET)
  list(APPEND runtimePath "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin")
endif()
message(STATUS "Kernel variant ${DRP_KERNEL_VARIANT} (patches: ${kernelPatches}), runtime ${kernelBin}")

# Patch 2's API: present only on kernels carrying source-contacts-experimental.patch.
include(CheckCXXSourceCompiles)
set(CMAKE_REQUIRED_INCLUDES ${Geant4_INCLUDE_DIRS})
set(CMAKE_REQUIRED_LIBRARIES ${Geant4_LIBRARIES})
set(CMAKE_REQUIRED_DEFINITIONS ${Geant4_DEFINITIONS})
check_cxx_source_compiles([=[
#include "G4DNAIndependentReactionTimeStepper.hh"
int main() { G4DNAIndependentReactionTimeStepper::RegisterSourceActivation(1, 0.); return 0; }
]=] DRP_HAS_SOURCE_ACTIVATION)
unset(CMAKE_REQUIRED_INCLUDES)
unset(CMAKE_REQUIRED_LIBRARIES)
unset(CMAKE_REQUIRED_DEFINITIONS)
if(DRP_HAS_SOURCE_ACTIVATION)
  set(hasSourceActivation 1)
else()
  set(hasSourceActivation 0)
endif()
message(STATUS "RegisterSourceActivation available: ${hasSourceActivation}")

# Application sources for fixtures that need the project chemistry (no HDF5).
get_filename_component(projectRoot "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
file(GLOB_RECURSE projectSources "${projectRoot}/src/*.cc")
list(FILTER projectSources EXCLUDE REGEX "/(MesoSpatialFile|Hdf5TableFile)[.]cc$")

# Job Object launcher: memory cap for fixtures whose defect exhausts memory.
add_executable(JobLauncher JobLauncher.cc)
target_link_libraries(JobLauncher PRIVATE psapi)

# drp_fixture(<name> <patch> <source> [CAP_MIB <MiB> SECONDS <s>] [PROJECT])
# PROJECT links the application sources (chemistry, physics, geometry).
function(drp_fixture name patch source)
  cmake_parse_arguments(arg "PROJECT" "CAP_MIB;SECONDS" "" ${ARGN})
  if(arg_PROJECT)
    add_executable(${name} ${source} ${projectSources})
    target_include_directories(${name} PRIVATE "${projectRoot}/header")
    target_compile_definitions(${name} PRIVATE DNACHEM_HAS_HDF5=0)
  else()
    add_executable(${name} ${source})
  endif()
  target_compile_definitions(${name} PRIVATE HAS_SOURCE_ACTIVATION=${hasSourceActivation})
  target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
  target_link_libraries(${name} PRIVATE ${Geant4_LIBRARIES})
  if(patch IN_LIST kernelPatches)
    set(expect fixed)
  else()
    set(expect defect)
  endif()
  if(arg_CAP_MIB)
    add_test(NAME ${name} COMMAND JobLauncher ${arg_CAP_MIB} ${arg_SECONDS}
             "${CMAKE_BINARY_DIR}/${name}.child" $<TARGET_FILE:${name}> --expect ${expect})
  else()
    add_test(NAME ${name} COMMAND ${name} --expect ${expect})
  endif()
  set(mods "")
  foreach(dir IN LISTS runtimePath)
    list(APPEND mods "PATH=path_list_prepend:${dir}")
  endforeach()
  set_tests_properties(${name} PROPERTIES ENVIRONMENT_MODIFICATION "${mods}"
                                          LABELS "patch${patch};${expect}")
endfunction()

drp_fixture(FinderCoincidentFixture 1 FinderCoincidentFixture.cc CAP_MIB 256 SECONDS 60)
drp_fixture(SourceContactFixture 2 SourceContactFixture.cc PROJECT)
drp_fixture(ReboundFoldFixture 3 ReboundFoldFixture.cc)

file(WRITE "${CMAKE_BINARY_DIR}/kernel-variant.txt" "${DRP_KERNEL_VARIANT}\n${Geant4_DIR}\n")
