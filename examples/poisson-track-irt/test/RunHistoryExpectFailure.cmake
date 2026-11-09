# Runs `history` on inputs that must be refused, and requires a nonzero exit
# status (EXPECT_STATUS when given), a HISTORY_ERROR line matching EXPECT_ERROR
# (regex), and no output file.
#   -DHISTORY=<exe> -DWORK_DIR=<dir> -DLIBRARY=<dir> -DCONFIG=<lines> [-DSCHEDULE=<rows>]
#   [-DPACKET_FIND=<text> -DPACKET_REPLACE=<text>]   (inputs: see HistoryTestInput.cmake)
#   -DEXPECT_ERROR=<regex> [-DEXPECT_STATUS=<n>]
if(NOT DEFINED EXPECT_ERROR)
  message(FATAL_ERROR "RunHistoryExpectFailure.cmake: EXPECT_ERROR is not set")
endif()
include("${CMAKE_CURRENT_LIST_DIR}/HistoryTestInput.cmake")

execute_process(
  COMMAND "${HISTORY}" --config "${historyConfig}" --output "${historyOutput}"
  RESULT_VARIABLE result
  OUTPUT_VARIABLE out
  ERROR_VARIABLE err)
file(WRITE "${WORK_DIR}/history.log" "${out}\n--- stderr ---\n${err}")
if(result EQUAL 0)
  message(FATAL_ERROR "history succeeded but must fail:\n${out}")
endif()
string(REGEX MATCH "HISTORY_ERROR[^\n]*" errorLine "${err}")
if(errorLine STREQUAL "")
  message(FATAL_ERROR "history failed (${result}) without a HISTORY_ERROR line:\n${err}\n${out}")
endif()
message(STATUS "exit ${result}: ${errorLine}")
if(NOT errorLine MATCHES "${EXPECT_ERROR}")
  message(FATAL_ERROR "unexpected error, wanted /${EXPECT_ERROR}/")
endif()
if(DEFINED EXPECT_STATUS AND NOT result EQUAL EXPECT_STATUS)
  message(FATAL_ERROR "exit status ${result}, expected ${EXPECT_STATUS}")
endif()
foreach(file species.csv activations.csv ledger.json)
  if(EXISTS "${historyOutput}/${file}")
    message(FATAL_ERROR "a refused history wrote ${file}")
  endif()
endforeach()
