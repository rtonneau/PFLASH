# Runs a doseCutoff `history` with `snapshots = true` and checks snapshots.h5.
#   -DHISTORY=<exe> -DWORK_DIR=<dir> -DLIBRARY=<dir> -DCONFIG=<lines>
#       (inputs: see HistoryTestInput.cmake; CONFIG must set mode = doseCutoff)
#   -DSNAPSHOTS_CHECK=<exe>   the snapshots-check executable; empty when the
#                             build has no HDF5
#   -DBOX_HALF_SIDE_NM=<h>    the reflecting half box
# With HDF5: the run with `snapshots = true` writes snapshots.h5, which
# snapshots-check validates (frames, counts against species.csv, positions in
# the box); the same config without the key writes no HDF5 file. Without HDF5:
# `snapshots = true` is a config error (exit status 2) and nothing is written.
foreach(var SNAPSHOTS_CHECK BOX_HALF_SIDE_NM)
  if(NOT DEFINED ${var})
    message(FATAL_ERROR "RunHistorySnapshots.cmake: ${var} is not set")
  endif()
endforeach()
set(baseConfig "${CONFIG}")

# ---- Without the key: no snapshots.h5 ---------------------------------------
set(WORK_DIR "${WORK_DIR}/plain")
set(CONFIG "${baseConfig}")
include("${CMAKE_CURRENT_LIST_DIR}/HistoryTestInput.cmake")
execute_process(
  COMMAND "${HISTORY}" --config "${historyConfig}" --output "${historyOutput}"
  RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "history without snapshots failed (${result}):\n${err}\n${out}")
endif()
if(EXISTS "${historyOutput}/snapshots.h5")
  message(FATAL_ERROR "a history without `snapshots = true` wrote snapshots.h5")
endif()
if(out MATCHES "snapshotFrames=")
  message(FATAL_ERROR "a history without `snapshots = true` reports snapshotFrames")
endif()

# ---- With the key -------------------------------------------------------------
set(WORK_DIR "${WORK_DIR}/../with")
set(CONFIG "${baseConfig}|snapshots = true")
include("${CMAKE_CURRENT_LIST_DIR}/HistoryTestInput.cmake")
execute_process(
  COMMAND "${HISTORY}" --config "${historyConfig}" --output "${historyOutput}"
  RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err)
file(WRITE "${WORK_DIR}/history.log" "${out}\n--- stderr ---\n${err}")

if(SNAPSHOTS_CHECK STREQUAL "")
  if(NOT result EQUAL 2)
    message(FATAL_ERROR "without HDF5, `snapshots = true` must exit 2, got ${result}:\n${err}\n${out}")
  endif()
  if(NOT err MATCHES "HDF5")
    message(FATAL_ERROR "the error does not mention HDF5:\n${err}")
  endif()
  if(EXISTS "${historyOutput}/snapshots.h5")
    message(FATAL_ERROR "a rejected config wrote snapshots.h5")
  endif()
  return()
endif()

if(NOT result EQUAL 0)
  message(FATAL_ERROR "history with snapshots failed (${result}):\n${err}\n${out}")
endif()
string(REGEX MATCH "HISTORY_DONE[^\n]*" doneLine "${out}")
message(STATUS "${doneLine}")
if(NOT doneLine MATCHES " snapshotFrames=([0-9]+)")
  message(FATAL_ERROR "the HISTORY_DONE line has no snapshotFrames:\n${out}")
endif()
set(frameCount "${CMAKE_MATCH_1}")
if(frameCount LESS 10)
  message(FATAL_ERROR "only ${frameCount} snapshot frames")
endif()
if(NOT EXISTS "${historyOutput}/snapshots.h5")
  message(FATAL_ERROR "history did not write snapshots.h5")
endif()
file(READ "${historyOutput}/ledger.json" ledger)
foreach(check "\"allPassed\": true" "\"snapshots\": " "\"enabled\": true" "\"snapshots.h5\"")
  string(FIND "${ledger}" "${check}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "ledger.json lacks ${check}")
  endif()
endforeach()

execute_process(
  COMMAND "${SNAPSHOTS_CHECK}" --snapshots "${historyOutput}/snapshots.h5"
          --species "${historyOutput}/species.csv" --activations "${historyOutput}/activations.csv"
          --box-half-side-nm "${BOX_HALF_SIDE_NM}"
  RESULT_VARIABLE checkResult OUTPUT_VARIABLE checkOut ERROR_VARIABLE checkErr)
file(WRITE "${WORK_DIR}/snapshots-check.log" "${checkOut}\n--- stderr ---\n${checkErr}")
message(STATUS "${checkOut}")
if(NOT checkResult EQUAL 0 OR NOT checkOut MATCHES "SNAPSHOTS_CHECK_OK")
  message(FATAL_ERROR "snapshots-check failed (${checkResult}):\n${checkErr}\n${checkOut}")
endif()
file(SIZE "${historyOutput}/snapshots.h5" snapshotBytes)
message(STATUS "snapshots.h5: ${snapshotBytes} bytes, ${frameCount} frames")
