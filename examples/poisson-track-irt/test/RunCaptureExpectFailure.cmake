# Runs capture with arguments that must be refused, and requires a nonzero exit
# status and a CAPTURE_ERROR line matching EXPECT_ERROR (regex). Nothing may be
# captured.
#   -DCAPTURE=<exe> -DOUTPUT_DIR=<dir> -DEXPECT_ERROR=<regex> -DARGS=<a;b;...>
foreach(var CAPTURE OUTPUT_DIR EXPECT_ERROR ARGS)
  if(NOT DEFINED ${var})
    message(FATAL_ERROR "RunCaptureExpectFailure.cmake: ${var} is not set")
  endif()
endforeach()

file(REMOVE_RECURSE "${OUTPUT_DIR}")
string(REPLACE "|" ";" argList "${ARGS}")
execute_process(
  COMMAND "${CAPTURE}" ${argList}
  RESULT_VARIABLE result
  OUTPUT_VARIABLE out
  ERROR_VARIABLE err)
if(result EQUAL 0)
  message(FATAL_ERROR "capture succeeded but must fail:\n${out}")
endif()
string(REGEX MATCH "CAPTURE_ERROR[^\n]*" errorLine "${err}")
if(errorLine STREQUAL "")
  message(FATAL_ERROR "capture failed (${result}) without a CAPTURE_ERROR line:\n${err}\n${out}")
endif()
message(STATUS "exit ${result}: ${errorLine}")
if(NOT errorLine MATCHES "${EXPECT_ERROR}")
  message(FATAL_ERROR "unexpected error, wanted /${EXPECT_ERROR}/")
endif()
if(EXISTS "${OUTPUT_DIR}/library.csv" OR EXISTS "${OUTPUT_DIR}/packet-00000.txt")
  message(FATAL_ERROR "a refused capture wrote packets")
endif()
