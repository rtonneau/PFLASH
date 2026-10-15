# Indicate miminum version and what error to raise if not satisfied
cmake_minimum_required(VERSION 3.16...3.27)

# Name the project
project(Simulation)

set(CMAKE_CXX_STANDARD 20)

# Look for geant4 paths (normaly sets by sourcing: ~/software/geant4/geant4-v*.*.*-install/share/Geant4/geant4make/geant4make.sh
find_package(Geant4 REQUIRED ui_all vis_all)
find_package(HDF5 REQUIRED COMPONENTS C CXX) # <-- must request CXX

#Look for toml++ library (installed by vcpkg)
#find_package(tomlplusplus CONFIG REQUIRED)

include(${Geant4_USE_FILE})

# Tell CMake to take all cc/hh files recursively (modular layout support)
file(GLOB_RECURSE sources CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/src/*.cc)
file(GLOB_RECURSE headers CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/header/*.hh)

# Project headers are included by their path under header/,
# e.g. #include "chemistry/DnaChemistryList.hh".
set(project_include_dirs ${PROJECT_SOURCE_DIR}/header)

# Tell CMake the name of the executable to produce.
# sim.cc will be the main source code.
add_executable(sim sim.cc ${sources} ${headers})

# Still need to link all Geant4 lib
target_link_libraries(sim ${Geant4_LIBRARIES} hdf5::hdf5-shared
    hdf5::hdf5_cpp-shared)
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
add_executable(MesoSpatialFileTest test/MesoSpatialFileTest.cc src/scoring/MesoSpatialFile.cc)
target_link_libraries(MesoSpatialFileTest hdf5::hdf5-shared hdf5::hdf5_cpp-shared)
target_include_directories(MesoSpatialFileTest PRIVATE ${project_include_dirs})
add_test(NAME MesoSpatialFileTest COMMAND MesoSpatialFileTest)

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

# Standalone unit test for MacroResolver (standard library only).
add_executable(MacroResolverTest test/MacroResolverTest.cc src/core/MacroResolver.cc)
target_include_directories(MacroResolverTest PRIVATE ${project_include_dirs})
add_test(NAME MacroResolverTest COMMAND MacroResolverTest)

# Standalone unit test for ResultsIndex (pure index builder on DataNode, std only).
add_executable(ResultsIndexTest test/ResultsIndexTest.cc src/scoring/ResultsIndex.cc src/scoring/DataNode.cc src/scoring/JsonWriter.cc)
target_include_directories(ResultsIndexTest PRIVATE ${project_include_dirs})
add_test(NAME ResultsIndexTest COMMAND ResultsIndexTest)
