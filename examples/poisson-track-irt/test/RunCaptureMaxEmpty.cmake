# CaptureMaxEmpty: in a box most primaries cross without a precursor molecule,
# capture COUNT packets with --max-empty MAX_EMPTY. Requires success, COUNT valid
# packets, at least one skipped event listed in capture.json, and packet seeds that
# are distinct, increasing and not reused by a skipped event.
#   -DCAPTURE=<exe> -DCHECK=<exe> -DMACRO=<file> -DOUTPUT_DIR=<dir> -DCOUNT=<n>
#   -DFIRST_SEED=<s> -DMAX_EMPTY=<m>
foreach(var CAPTURE CHECK MACRO OUTPUT_DIR COUNT FIRST_SEED MAX_EMPTY)
  if(NOT DEFINED ${var})
    message(FATAL_ERROR "RunCaptureMaxEmpty.cmake: ${var} is not set")
  endif()
endforeach()

file(REMOVE_RECURSE "${OUTPUT_DIR}")
execute_process(
  COMMAND "${CAPTURE}" --macro "${MACRO}" --output "${OUTPUT_DIR}" --count ${COUNT}
          --first-seed ${FIRST_SEED} --max-empty ${MAX_EMPTY}
  RESULT_VARIABLE captureResult
  OUTPUT_VARIABLE captureOut
  ERROR_VARIABLE captureErr)
file(WRITE "${OUTPUT_DIR}.capture.log" "${captureOut}\n--- stderr ---\n${captureErr}")
if(NOT captureResult EQUAL 0)
  message(FATAL_ERROR "capture failed (${captureResult}):\n${captureErr}")
endif()

execute_process(
  COMMAND "${CHECK}" "${OUTPUT_DIR}" --expect-count ${COUNT}
  RESULT_VARIABLE checkResult
  OUTPUT_VARIABLE checkOut
  ERROR_VARIABLE checkErr)
if(NOT checkResult EQUAL 0 OR NOT checkOut MATCHES "CAPTURE_CHECK_OK packets=${COUNT} ")
  message(FATAL_ERROR "capture-check failed (${checkResult}):\n${checkErr}\n${checkOut}")
endif()

file(READ "${OUTPUT_DIR}/capture.json" summary)
string(JSON skippedCount LENGTH "${summary}" skippedEmptyEvents)
if(skippedCount LESS 1)
  message(FATAL_ERROR "no skipped empty event listed; the test box is too large")
endif()
string(JSON maxEmpty GET "${summary}" maxEmptyEvents)
if(NOT maxEmpty EQUAL MAX_EMPTY)
  message(FATAL_ERROR "capture.json maxEmptyEvents is ${maxEmpty}, expected ${MAX_EMPTY}")
endif()
set(seeds "")
math(EXPR lastSkipped "${skippedCount} - 1")
foreach(i RANGE ${lastSkipped})
  string(JSON seed GET "${summary}" skippedEmptyEvents ${i} physicsSeed)
  list(APPEND seeds ${seed})
endforeach()
set(previous 0)
math(EXPR lastPacket "${COUNT} - 1")
foreach(i RANGE ${lastPacket})
  string(JSON seed GET "${summary}" packets ${i} physicsSeed)
  if(NOT seed GREATER previous)
    message(FATAL_ERROR "packet ${i} seed ${seed} does not increase (previous ${previous})")
  endif()
  if(seed IN_LIST seeds)
    message(FATAL_ERROR "packet ${i} seed ${seed} is also a skipped event's seed")
  endif()
  set(previous ${seed})
endforeach()
message(STATUS "CaptureMaxEmpty: ${COUNT} packets, ${skippedCount} skipped empty events, last seed ${previous}")
