# CaptureSmoke: capture COUNT packets into OUTPUT_DIR, then read them back and
# validate them with capture-check (format, tag, energy ledger, count).
#   -DCAPTURE=<exe> -DCHECK=<exe> -DMACRO=<file> -DOUTPUT_DIR=<dir> -DCOUNT=<n> -DFIRST_SEED=<s>
#   [-DREQUIRE_ESCAPE=ON]  also require escaped energy > 0 in every packet
foreach(var CAPTURE CHECK MACRO OUTPUT_DIR COUNT FIRST_SEED)
  if(NOT DEFINED ${var})
    message(FATAL_ERROR "RunCaptureSmoke.cmake: ${var} is not set")
  endif()
endforeach()

file(REMOVE_RECURSE "${OUTPUT_DIR}")
execute_process(
  COMMAND "${CAPTURE}" --macro "${MACRO}" --output "${OUTPUT_DIR}" --count ${COUNT} --first-seed ${FIRST_SEED}
  RESULT_VARIABLE captureResult
  OUTPUT_VARIABLE captureOut
  ERROR_VARIABLE captureErr)
file(WRITE "${OUTPUT_DIR}.capture.log" "${captureOut}\n--- stderr ---\n${captureErr}")
if(NOT captureResult EQUAL 0)
  message(FATAL_ERROR "capture failed (${captureResult}):\n${captureErr}\n${captureOut}")
endif()
string(REGEX MATCH "CAPTURE_DONE[^\n]*" doneLine "${captureOut}")
if(doneLine STREQUAL "")
  message(FATAL_ERROR "capture printed no CAPTURE_DONE line:\n${captureOut}")
endif()
message(STATUS "${doneLine}")

foreach(file library.csv capture.json packet-00000.txt)
  if(NOT EXISTS "${OUTPUT_DIR}/${file}")
    message(FATAL_ERROR "capture did not write ${file}")
  endif()
endforeach()

execute_process(
  COMMAND "${CHECK}" "${OUTPUT_DIR}" --expect-count ${COUNT}
  RESULT_VARIABLE checkResult
  OUTPUT_VARIABLE checkOut
  ERROR_VARIABLE checkErr)
message(STATUS "capture-check:\n${checkOut}")
if(NOT checkResult EQUAL 0)
  message(FATAL_ERROR "capture-check failed (${checkResult}):\n${checkErr}")
endif()
if(NOT checkOut MATCHES "CAPTURE_CHECK_OK packets=${COUNT} ")
  message(FATAL_ERROR "capture-check did not report ${COUNT} valid packets")
endif()

if(REQUIRE_ESCAPE)
  string(REGEX MATCHALL "escapedEv=[^ ]+" escapes "${checkOut}")
  list(LENGTH escapes escapeCount)
  if(NOT escapeCount EQUAL COUNT)
    message(FATAL_ERROR "capture-check listed ${escapeCount} packets, expected ${COUNT}")
  endif()
  foreach(escape IN LISTS escapes)
    if(escape STREQUAL "escapedEv=0")
      message(FATAL_ERROR "a packet has no escaped energy; the escape test needs a box smaller than the track")
    endif()
  endforeach()
endif()
