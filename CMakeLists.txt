# Indicate miminum version and what error to raise if not satisfied
cmake_minimum_required(VERSION 3.16...3.27)

# Name the project
project(Simulation)

set(CMAKE_CXX_STANDARD 20)

# Look for geant4 paths (normaly sets by sourcing: ~/software/geant4/geant4-v*.*.*-install/share/Geant4/geant4make/geant4make.sh
find_package(Geant4 REQUIRED ui_all vis_all)

# HDF5 is optional: it is only needed for the mesoscopic spatial snapshots
# (SpeciesMesoSpatial.h5). AUTO uses it when found, ON requires it, OFF does not
# search for it (a stub of MesoSpatialFile is built instead).
set(DNACHEM_WITH_HDF5 "AUTO" CACHE STRING "HDF5 support for SpeciesMesoSpatial.h5: AUTO, ON or OFF")
set_property(CACHE DNACHEM_WITH_HDF5 PROPERTY STRINGS AUTO ON OFF)
string(TOUPPER "${DNACHEM_WITH_HDF5}" _dnachemWithHdf5)
if(_dnachemWithHdf5 STREQUAL "ON")
  find_package(HDF5 REQUIRED COMPONENTS C CXX) # <-- must request CXX
elseif(_dnachemWithHdf5 STREQUAL "AUTO")
  find_package(HDF5 COMPONENTS C CXX) # <-- must request CXX
  if(NOT HDF5_FOUND)
    message(STATUS "HDF5 not found: building without HDF5 (SpeciesMesoSpatial.h5 unavailable)")
  endif()
elseif(_dnachemWithHdf5 STREQUAL "OFF")
  message(STATUS "HDF5 disabled (DNACHEM_WITH_HDF5=OFF): SpeciesMesoSpatial.h5 unavailable")
else()
  message(FATAL_ERROR "DNACHEM_WITH_HDF5 must be AUTO, ON or OFF (got '${DNACHEM_WITH_HDF5}')")
endif()
if(_dnachemWithHdf5 STREQUAL "OFF")
  set(DNACHEM_HAS_HDF5 FALSE)
elseif(HDF5_FOUND)
  set(DNACHEM_HAS_HDF5 TRUE)
else()
  set(DNACHEM_HAS_HDF5 FALSE)
endif()

#Look for toml++ library (installed by vcpkg)
#find_package(tomlplusplus CONFIG REQUIRED)

include(${Geant4_USE_FILE})

# Tell CMake to take all cc/hh files recursively (modular layout support)
file(GLOB_RECURSE sources CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/src/*.cc)
file(GLOB_RECURSE headers CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/header/*.hh)

# Project headers are included by their path under header/,
# e.g. #include "chemistry/DnaChemistryList.hh".
set(project_include_dirs ${PROJECT_SOURCE_DIR}/header)

# Exactly one MesoSpatialFile implementation is compiled: the HDF5 one, or the
# std-only stub that reports the feature as unavailable.
if(DNACHEM_HAS_HDF5)
  list(FILTER sources EXCLUDE REGEX "/src/scoring/MesoSpatialFileNoHdf5[.]cc$")
else()
  list(FILTER sources EXCLUDE REGEX "/src/scoring/MesoSpatialFile[.]cc$")
endif()

# Tell CMake the name of the executable to produce.
# sim.cc will be the main source code.
add_executable(sim sim.cc ${sources} ${headers})

# Still need to link all Geant4 lib
target_link_libraries(sim ${Geant4_LIBRARIES})
if(DNACHEM_HAS_HDF5)
  target_link_libraries(sim hdf5::hdf5-shared hdf5::hdf5_cpp-shared)
  target_compile_definitions(sim PRIVATE DNACHEM_HAS_HDF5=1)
else()
  target_compile_definitions(sim PRIVATE DNACHEM_HAS_HDF5=0)
endif()
target_include_directories(sim PRIVATE ${project_include_dirs})

add_custom_target(Simulation DEPENDS sim)

# Standalone unit test for ArgParser (no Geant4 runtime init required).
enable_testing()
add_executable(ArgParserTest test/ArgParserTest.cc src/core/ArgParser.cc)
target_link_libraries(ArgParserTest ${Geant4_LIBRARIES})
target_include_directories(ArgParserTest PRIVATE ${project_include_dirs})
add_test(NAME ArgParserTest COMMAND ArgParserTest)

# Standalone unit test for OutputDir (no Geant4 runtime init required).
add_executable(OutputDirTest test/OutputDirTest.cc src/core/OutputDir.cc)
target_link_libraries(OutputDirTest ${Geant4_LIBRARIES})
target_include_directories(OutputDirTest PRIVATE ${project_include_dirs})
add_test(NAME OutputDirTest COMMAND OutputDirTest)

# Standalone unit test for ReactionTableDump::FormatReactionLabel (pure string
# logic, no Geant4 runtime init required).
add_executable(ReactionTableDumpTest test/ReactionTableDumpTest.cc src/chemistry/ReactionTableDump.cc src/core/DnaLogger.cc)
target_link_libraries(ReactionTableDumpTest ${Geant4_LIBRARIES})
target_include_directories(ReactionTableDumpTest PRIVATE ${project_include_dirs})
add_test(NAME ReactionTableDumpTest COMMAND ReactionTableDumpTest)

# Standalone unit test for the CatalogExport::Make* node builders (no Geant4
# runtime init required).
add_executable(CatalogExportTest test/CatalogExportTest.cc src/chemistry/CatalogExport.cc src/core/DnaLogger.cc src/scoring/DataNode.cc src/scoring/JsonWriter.cc)
target_link_libraries(CatalogExportTest ${Geant4_LIBRARIES})
target_include_directories(CatalogExportTest PRIVATE ${project_include_dirs})
add_test(NAME CatalogExportTest COMMAND CatalogExportTest)

# Standalone unit test for ReactionCounter (pure accumulation/binning logic,
# no Geant4 runtime init required).
add_executable(ReactionCounterTest test/ReactionCounterTest.cc src/scoring/ReactionCounter.cc src/core/OutputDir.cc)
target_link_libraries(ReactionCounterTest ${Geant4_LIBRARIES})
target_include_directories(ReactionCounterTest PRIVATE ${project_include_dirs})
add_test(NAME ReactionCounterTest COMMAND ReactionCounterTest)

# Standalone unit test for PhysicsInteractionCounter (fully generic,
# portable, no dnachem-min-specific dependency -- no OutputDir.cc needed).
add_executable(PhysicsInteractionCounterTest test/PhysicsInteractionCounterTest.cc src/scoring/PhysicsInteractionCounter.cc)
target_link_libraries(PhysicsInteractionCounterTest ${Geant4_LIBRARIES})
target_include_directories(PhysicsInteractionCounterTest PRIVATE ${project_include_dirs})
add_test(NAME PhysicsInteractionCounterTest COMMAND PhysicsInteractionCounterTest)

# Standalone unit test for TrackLengthTable (portable, standard library only).
add_executable(TrackLengthTableTest test/TrackLengthTableTest.cc src/scoring/TrackLengthTable.cc)
target_include_directories(TrackLengthTableTest PRIVATE ${project_include_dirs})
add_test(NAME TrackLengthTableTest COMMAND TrackLengthTableTest)

# Standalone unit test for DataNode + JsonWriter (portable ordered tree and
# its generic JSON serialiser, std only -- no Geant4 link needed).
add_executable(JsonWriterTest test/JsonWriterTest.cc src/scoring/DataNode.cc src/scoring/JsonWriter.cc)
target_include_directories(JsonWriterTest PRIVATE ${project_include_dirs})
add_test(NAME JsonWriterTest COMMAND JsonWriterTest)

# Standalone unit test for RunAccumulator (pure accumulation/prefix
# bookkeeping logic, no Geant4 kernel dependency -- DumpAndReset's
# file-writing/species lookup lives in RunAccumulatorMessenger instead,
# which is not unit-tested, matching this project's existing convention).
add_executable(RunAccumulatorTest test/RunAccumulatorTest.cc src/scoring/RunAccumulator.cc src/scoring/ReactionCounter.cc src/scoring/PhysicsInteractionCounter.cc src/scoring/MesoSpeciesCounter.cc src/scoring/TrackLengthTable.cc src/scoring/DataNode.cc src/core/OutputDir.cc)
target_link_libraries(RunAccumulatorTest ${Geant4_LIBRARIES})
target_include_directories(RunAccumulatorTest PRIVATE ${project_include_dirs})
add_test(NAME RunAccumulatorTest COMMAND RunAccumulatorTest)

# Standalone unit test for PreChemicalFiles (file-name and staging-move logic,
# standard library only).
add_executable(PreChemicalFilesTest test/PreChemicalFilesTest.cc src/scoring/PreChemicalFiles.cc)
target_link_libraries(PreChemicalFilesTest ${Geant4_LIBRARIES})
target_include_directories(PreChemicalFilesTest PRIVATE ${project_include_dirs})
add_test(NAME PreChemicalFilesTest COMMAND PreChemicalFilesTest)

# Standalone unit test for ChemistryRegistry (pure name lookup/selection logic,
# no Geant4 kernel dependency -- it only stores builder function pointers).
add_executable(ChemistryRegistryTest test/ChemistryRegistryTest.cc src/chemistry/ChemistryRegistry.cc)
target_link_libraries(ChemistryRegistryTest ${Geant4_LIBRARIES})
target_include_directories(ChemistryRegistryTest PRIVATE ${project_include_dirs})
add_test(NAME ChemistryRegistryTest COMMAND ChemistryRegistryTest)

# Standalone unit test for ScavengerSpec (parsing /chem/env/scavenger values,
# unit conversion and checks -- pure logic, no Geant4 kernel dependency).
add_executable(ScavengerSpecTest test/ScavengerSpecTest.cc src/geometry/ScavengerSpec.cc)
target_link_libraries(ScavengerSpecTest ${Geant4_LIBRARIES})
target_include_directories(ScavengerSpecTest PRIVATE ${project_include_dirs})
add_test(NAME ScavengerSpecTest COMMAND ScavengerSpecTest)

# Standalone unit test for MesoSettings (pixel count, log time grid -- pure
# logic, std only).
add_executable(MesoSettingsTest test/MesoSettingsTest.cc src/chemistry/MesoSettings.cc)
target_include_directories(MesoSettingsTest PRIVATE ${project_include_dirs})
add_test(NAME MesoSettingsTest COMMAND MesoSettingsTest)

# Standalone unit test for MesoSpeciesCounter (per-time species counts of the
# mesoscopic stage -- pure logic, std only).
add_executable(MesoSpeciesCounterTest test/MesoSpeciesCounterTest.cc src/scoring/MesoSpeciesCounter.cc)
target_include_directories(MesoSpeciesCounterTest PRIVATE ${project_include_dirs})
add_test(NAME MesoSpeciesCounterTest COMMAND MesoSpeciesCounterTest)

# Standalone unit test for MesoSpatialFile (HDF5 writer of the mesoscopic
# spatial snapshots -- std + HDF5 only, no Geant4).
# Added only when HDF5 is on; without it the stub is tested instead.
if(DNACHEM_HAS_HDF5)
  add_executable(MesoSpatialFileTest test/MesoSpatialFileTest.cc src/scoring/MesoSpatialFile.cc)
  target_link_libraries(MesoSpatialFileTest hdf5::hdf5-shared hdf5::hdf5_cpp-shared)
  target_include_directories(MesoSpatialFileTest PRIVATE ${project_include_dirs})
  add_test(NAME MesoSpatialFileTest COMMAND MesoSpatialFileTest)
else()
  # Standalone unit test for the no-HDF5 stub of MesoSpatialFile (std only).
  add_executable(MesoSpatialFileNoHdf5Test test/MesoSpatialFileNoHdf5Test.cc src/scoring/MesoSpatialFileNoHdf5.cc)
  target_include_directories(MesoSpatialFileNoHdf5Test PRIVATE ${project_include_dirs})
  add_test(NAME MesoSpatialFileNoHdf5Test COMMAND MesoSpatialFileNoHdf5Test)
endif()

# Standalone unit test for Tonneau2025Table (Table 2 of Tonneau et al. 2025 as
# plain data) and the Geant4-free part of Tonneau2025Reactions (routing, types,
# bulk-reaction list). Links Geant4 but needs no Geant4 kernel.
add_executable(Tonneau2025TableTest test/Tonneau2025TableTest.cc src/chemistry/catalog/Tonneau2025Table.cc src/chemistry/catalog/Tonneau2025Reactions.cc)
target_link_libraries(Tonneau2025TableTest ${Geant4_LIBRARIES})
target_include_directories(Tonneau2025TableTest PRIVATE ${project_include_dirs})
add_test(NAME Tonneau2025TableTest COMMAND Tonneau2025TableTest)

# Copy macro files AFTER the target is built
# Reason: file(COPY) runs at configure time, but
# CMAKE_BUILD_TYPE is known at build time with MSVC.
add_custom_command(
    TARGET sim POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${PROJECT_SOURCE_DIR}/macro/"
    "$<TARGET_FILE_DIR:sim>/macro"
    COMMENT "Copying macro files to output directory..."
)

# ---- Release package (GitHub Release asset, built locally, see docs/release.md) ----
# `cpack` in the run build dir produces dnachem-min-win64.zip: sim.exe, the two
# HDF5 DLLs (vcpkg, no further dependency), macro/, a README and an environment
# check. Geant4, Qt and the MSVC runtime are NOT shipped: the user's own Geant4
# install provides them (sim.exe loads the G4*.dll by name, so it must be the
# same Geant4 version and build options, see packaging/README.txt).
install(TARGETS sim RUNTIME DESTINATION .)
if(DNACHEM_HAS_HDF5)
  install(FILES $<TARGET_FILE:hdf5::hdf5-shared> $<TARGET_FILE:hdf5::hdf5_cpp-shared>
          DESTINATION .)
endif()
install(DIRECTORY macro/ DESTINATION macro
        FILES_MATCHING PATTERN "*.in" PATTERN "*.mac")
install(FILES packaging/README.txt DESTINATION .)
if(WIN32)
  install(FILES packaging/check-env.ps1 DESTINATION .)
endif()
find_package(Git QUIET)
install(CODE "
  set(_version unknown)
  if(NOT \"${GIT_EXECUTABLE}\" STREQUAL \"\")
    execute_process(COMMAND \"${GIT_EXECUTABLE}\" describe --tags --always --dirty
                    WORKING_DIRECTORY \"${PROJECT_SOURCE_DIR}\"
                    OUTPUT_VARIABLE _version OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
  endif()
  file(WRITE \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/VERSION.txt\"
       \"dnachem-min \${_version}\\nbuilt against Geant4 ${Geant4_VERSION}\\n\")
")
set(CPACK_GENERATOR ZIP)
set(CPACK_PACKAGE_NAME dnachem-min)
if(WIN32)
  set(CPACK_PACKAGE_FILE_NAME dnachem-min-win64)
else()
  set(CPACK_PACKAGE_FILE_NAME dnachem-min-${CMAKE_SYSTEM_NAME})
endif()
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY ON)
include(CPack)

# Standalone unit test for MacroResolver (standard library only).
add_executable(MacroResolverTest test/MacroResolverTest.cc src/core/MacroResolver.cc)
target_include_directories(MacroResolverTest PRIVATE ${project_include_dirs})
add_test(NAME MacroResolverTest COMMAND MacroResolverTest)

# Standalone unit test for ResultsIndex (pure index builder on DataNode, std only).
add_executable(ResultsIndexTest test/ResultsIndexTest.cc src/scoring/ResultsIndex.cc src/scoring/DataNode.cc src/scoring/JsonWriter.cc)
target_include_directories(ResultsIndexTest PRIVATE ${project_include_dirs})
add_test(NAME ResultsIndexTest COMMAND ResultsIndexTest)

# Architecture explorer (docs/explorer): link, file-coverage and freshness check.
# Skipped when Node is not installed.
find_program(NODE_EXECUTABLE node)
if(NODE_EXECUTABLE)
  add_test(NAME ExplorerCheck
           COMMAND ${NODE_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/docs/explorer/check.js)
  # Macro builder model: command generation, ordering, compatibility, validation.
  add_test(NAME MacroBuilderTest
           COMMAND ${NODE_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/docs/explorer/macro-builder.test.js)
  # Catalogue view model: rows, k text, filtering.
  add_test(NAME CatalogViewTest
           COMMAND ${NODE_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/docs/explorer/catalog-view.test.js)
  # Pure comparison logic of the catalogue generator.
  add_test(NAME CatalogExportScriptTest
           COMMAND ${NODE_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/docs/explorer/export-catalog.test.js)
  # Drift check: committed catalog.json must equal the live chemistry of this sim.
  # A Debug sim built against the release Geant4 install mixes CRTs and crashes at
  # /run/initialize (heap corruption), so the check runs the RelWithDebInfo sim of
  # ../build when this is a Debug tree; override with -DCATALOG_EXPORT_SIM=<path>.
  # That path is used even when it does not exist yet: export-catalog.js then fails with
  # "sim not found", and it fails when the sim is older than any chemistry source.
  set(CATALOG_EXPORT_SIM "" CACHE FILEPATH "sim executable used by CatalogExportCheck (default: this target, or build/sim.exe for Debug)")
  if(CATALOG_EXPORT_SIM)
    set(_catalogSim "${CATALOG_EXPORT_SIM}")
  elseif(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(_catalogSim "${CMAKE_CURRENT_SOURCE_DIR}/build/sim${CMAKE_EXECUTABLE_SUFFIX}")
  else()
    set(_catalogSim "$<TARGET_FILE:sim>")
  endif()
  add_test(NAME CatalogExportCheck
           COMMAND ${NODE_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/docs/explorer/export-catalog.js
                   --check --sim ${_catalogSim})
endif()
