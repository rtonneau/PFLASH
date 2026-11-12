# StockBuildModuleCheck: the loaded-module list in capture.json proves which Geant4
# DLLs the stock build really ran with.
#   -DCAPTURE=<exe> -DMACRO=<file> -DOUTPUT_DIR=<dir> -DFIRST_SEED=<s> -DGEANT4_BIN_DIR=<dir>
# Requires loadedGeant4Modules to be non-empty and every path to lie under GEANT4_BIN_DIR
# (Geant4_DIR/../../../bin), compared case-insensitively with '/' separators.
foreach(var CAPTURE MACRO OUTPUT_DIR FIRST_SEED GEANT4_BIN_DIR)
  if(NOT DEFINED ${var})
    message(FATAL_ERROR "RunModuleCheck.cmake: ${var} is not set")
  endif()
endforeach()

file(REMOVE_RECURSE "${OUTPUT_DIR}")
execute_process(
  COMMAND "${CAPTURE}" --macro "${MACRO}" --output "${OUTPUT_DIR}" --count 1 --first-seed ${FIRST_SEED}
  RESULT_VARIABLE result
  OUTPUT_VARIABLE out
  ERROR_VARIABLE err)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "capture failed (${result}):\n${err}\n${out}")
endif()

file(READ "${OUTPUT_DIR}/capture.json" json)
string(JSON moduleCount LENGTH "${json}" loadedGeant4Modules)
if(moduleCount EQUAL 0)
  message(FATAL_ERROR "capture.json lists no loaded Geant4 module")
endif()

get_filename_component(binDir "${GEANT4_BIN_DIR}" ABSOLUTE)
file(TO_CMAKE_PATH "${binDir}" binDir)
string(TOLOWER "${binDir}" binDirLower)
string(REGEX REPLACE "/+$" "" binDirLower "${binDirLower}")

math(EXPR lastModule "${moduleCount} - 1")
foreach(i RANGE ${lastModule})
  string(JSON module GET "${json}" loadedGeant4Modules ${i})
  file(TO_CMAKE_PATH "${module}" modulePath)
  string(TOLOWER "${modulePath}" moduleLower)
  string(FIND "${moduleLower}" "${binDirLower}/" position)
  if(NOT position EQUAL 0)
    message(FATAL_ERROR "loaded Geant4 module '${module}' is not under ${binDir}")
  endif()
  message(STATUS "module ${i}: ${module}")
endforeach()
message(STATUS "${moduleCount} Geant4 modules, all under ${binDir}")
