cmake_minimum_required(VERSION 3.20)
project(CheckpointFeasibility LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(Geant4 11.4.3 EXACT REQUIRED)
include(${Geant4_USE_FILE})
get_filename_component(pflashRoot "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
set(PROBE_ARTIFACT_DIR "${CMAKE_BINARY_DIR}/artifacts" CACHE PATH "Probe evidence directory")
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/instrumented")

# Compile independent, instrumented copies of application sources. Never edit
# the application or the shared Geant4 sources/install. Fail if hooks drift.
function(insert_hook variable needle replacement)
  string(FIND "${${variable}}" "${needle}" position)
  if(position EQUAL -1)
    message(FATAL_ERROR "Probe hook no longer matches: ${needle}")
  endif()
  string(REPLACE "${needle}" "${replacement}" updated "${${variable}}")
  set(${variable} "${updated}" PARENT_SCOPE)
endfunction()
file(READ "${pflashRoot}/src/actions/PrimaryGeneratorAction.cc" primary)
insert_hook(primary "  this->fpParticleGun->GeneratePrimaryVertex(anEvent);"
  "  Probe::BeginEvent(anEvent->GetEventID());\n  this->fpParticleGun->GeneratePrimaryVertex(anEvent);")
file(WRITE "${CMAKE_BINARY_DIR}/instrumented/PrimaryGeneratorAction.cc" "#include \"ProbeHooks.hh\"\n${primary}")
file(READ "${pflashRoot}/src/chemistry/TimeStepAction.cc" chemistry)
insert_hook(chemistry "void TimeStepAction::StartProcessing()\n{"
  "void TimeStepAction::StartProcessing()\n{\n  Probe::StartChemistry();")
insert_hook(chemistry "void TimeStepAction::UserPreTimeStepAction()\n{"
  "void TimeStepAction::UserPreTimeStepAction()\n{\n  Probe::Particle(\"pre\");")
insert_hook(chemistry "void TimeStepAction::UserPostTimeStepAction()\n{"
  "void TimeStepAction::UserPostTimeStepAction()\n{\n  Probe::Particle(\"post\");")
insert_hook(chemistry "void TimeStepAction::EndProcessing()\n{"
  "void TimeStepAction::EndProcessing()\n{\n  Probe::EndChemistry();")
insert_hook(chemistry "  fpEventScheduler->Run();"
  "  Probe::BeforeMeso(*fpEventScheduler);\n  fpEventScheduler->Run();\n  Probe::AfterMeso(*fpEventScheduler);")
insert_hook(chemistry "  void Record(const G4DNAMesh &mesh, G4double time)\n  {"
  "  void Record(const G4DNAMesh &mesh, G4double time)\n  {\n    Probe::Mesh(mesh, time);")
insert_hook(chemistry "  const auto *molA = GetMolecule(a)->GetMolecularConfiguration();"
  "  Probe::Reaction(a, b);\n  const auto *molA = GetMolecule(a)->GetMolecularConfiguration();")
file(WRITE "${CMAKE_BINARY_DIR}/instrumented/TimeStepAction.cc" "#include \"ProbeHooks.hh\"\n${chemistry}")
file(GLOB_RECURSE appSources CONFIGURE_DEPENDS "${pflashRoot}/src/*.cc")
list(FILTER appSources EXCLUDE REGEX "/(PrimaryGeneratorAction|TimeStepAction|MesoSpatialFile|Hdf5TableFile)[.]cc$")
add_executable(CheckpointProbe CheckpointProbe.cc ProbeHooks.cc ${appSources}
  "${CMAKE_BINARY_DIR}/instrumented/PrimaryGeneratorAction.cc"
  "${CMAKE_BINARY_DIR}/instrumented/TimeStepAction.cc")
target_include_directories(CheckpointProbe PRIVATE "${pflashRoot}/header" "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_definitions(CheckpointProbe PRIVATE DNACHEM_HAS_HDF5=0)
target_link_libraries(CheckpointProbe PRIVATE ${Geant4_LIBRARIES})
add_executable(CompareCheckpointTrace CompareCheckpointTrace.cc)
enable_testing()
foreach(execution IN ITEMS serial2 mt2 mt4)
  if(execution STREQUAL "serial2")
    set(workers 0)
    set(events 2)
  elseif(execution STREQUAL "mt2")
    set(workers 2)
    set(events 2)
  else()
    set(workers 2)
    set(events 4)
  endif()
  foreach(mode IN ITEMS oracle particle meso)
    add_test(NAME ${mode}_${execution}
      COMMAND ${CMAKE_COMMAND}
        -DPROBE=$<TARGET_FILE:CheckpointProbe>
        -DCOMPARE=$<TARGET_FILE:CompareCheckpointTrace>
        -DMACRO=${CMAKE_CURRENT_SOURCE_DIR}/Probe.in
        -DARTIFACTS=${PROBE_ARTIFACT_DIR}/${mode}_${execution}
        -DMODE=${mode} -DWORKERS=${workers} -DEVENTS=${events}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/RunProbe.cmake)
    set_tests_properties(${mode}_${execution} PROPERTIES LABELS checkpoint-feasibility TIMEOUT 180)
  endforeach()
endforeach()
