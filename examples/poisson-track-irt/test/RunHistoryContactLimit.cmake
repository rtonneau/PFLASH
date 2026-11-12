# Runs `history` with newOldContactLimit exceeded and checks the one failure that
# still writes its outputs, then the same history with the limit at the observed
# count, which must pass.
#   -DHISTORY=<exe> -DWORK_DIR=<dir> -DLIBRARY=<dir> -DCONFIG=<lines> -DSCHEDULE=<rows>
#       (inputs: see HistoryTestInput.cmake). CONFIG must hold "newOldContactLimit = 0".
# Run 1 (limit 0) must exit 1 with
#   HISTORY_ERROR PTIRT_NEW_OLD_CONTACTS pairs=<n> limit=0
# on stderr, no HISTORY_DONE, species.csv / activations.csv / ledger.json written, and
# a ledger with checks.allPassed false, newOldContacts.pairs = n > 0, limit 0.
# Run 2 (limit n: pairs > limit is false) must exit 0 with checks.allPassed true and the
# same pair count (same seeds, same history).
include("${CMAKE_CURRENT_LIST_DIR}/HistoryTestInput.cmake")

function(ledger_value outVar ledgerText)
  string(JSON value ERROR_VARIABLE problem GET "${ledgerText}" ${ARGN})
  if(NOT problem STREQUAL "NOTFOUND")
    message(FATAL_ERROR "ledger.json: ${ARGN}: ${problem}")
  endif()
  set(${outVar} "${value}" PARENT_SCOPE)
endfunction()

# ---- run 1: the limit is exceeded ---------------------------------------------
execute_process(
  COMMAND "${HISTORY}" --config "${historyConfig}" --output "${historyOutput}"
  RESULT_VARIABLE result
  OUTPUT_VARIABLE out
  ERROR_VARIABLE err)
file(WRITE "${WORK_DIR}/history.log" "${out}\n--- stderr ---\n${err}")
if(NOT result EQUAL 1)
  message(FATAL_ERROR "exit status ${result}, expected 1 (limit exceeded):\n${err}\n${out}")
endif()
string(REGEX MATCH "HISTORY_ERROR PTIRT_NEW_OLD_CONTACTS pairs=([0-9]+) limit=0" errorLine "${err}")
if(errorLine STREQUAL "")
  message(FATAL_ERROR "no 'HISTORY_ERROR PTIRT_NEW_OLD_CONTACTS pairs=<n> limit=0' line:\n${err}")
endif()
set(reportedPairs "${CMAKE_MATCH_1}")
message(STATUS "exit ${result}: ${errorLine}")
if(out MATCHES "HISTORY_DONE")
  message(FATAL_ERROR "a history that exceeded the limit printed HISTORY_DONE")
endif()
foreach(file species.csv activations.csv ledger.json)
  if(NOT EXISTS "${historyOutput}/${file}")
    message(FATAL_ERROR "the exceeded-limit history did not write ${file}")
  endif()
endforeach()

file(READ "${historyOutput}/ledger.json" ledger)
ledger_value(allPassed "${ledger}" checks allPassed)
if(NOT allPassed STREQUAL "OFF")
  message(FATAL_ERROR "checks.allPassed is '${allPassed}', expected false")
endif()
ledger_value(limitPassed "${ledger}" checks newOldContactLimitPassed)
ledger_value(pairs "${ledger}" newOldContacts pairs)
ledger_value(limit "${ledger}" newOldContacts limit)
ledger_value(exceeded "${ledger}" newOldContacts limitExceeded)
ledger_value(formatVersion "${ledger}" formatVersion)
if(NOT limitPassed STREQUAL "OFF" OR NOT exceeded STREQUAL "ON" OR NOT limit EQUAL 0
   OR NOT pairs EQUAL reportedPairs OR NOT pairs GREATER 0 OR NOT formatVersion EQUAL 2)
  message(FATAL_ERROR "ledger: limitPassed=${limitPassed} limitExceeded=${exceeded} limit=${limit} "
                      "pairs=${pairs} (stderr said ${reportedPairs}) formatVersion=${formatVersion}")
endif()
ledger_value(cpuSeconds "${ledger}" newOldContacts cpuSeconds)
string(JSON activationCount LENGTH "${ledger}" newOldContacts perActivation)
if(NOT activationCount EQUAL 2)
  message(FATAL_ERROR "newOldContacts.perActivation has ${activationCount} entries, expected 2")
endif()
# The first source meets an empty history; the second meets the first.
ledger_value(firstPairs "${ledger}" newOldContacts perActivation 0 pairs)
ledger_value(secondPairs "${ledger}" newOldContacts perActivation 1 pairs)
if(NOT firstPairs EQUAL 0 OR NOT secondPairs EQUAL pairs)
  message(FATAL_ERROR "perActivation pairs: ${firstPairs}, ${secondPairs}; total ${pairs}")
endif()
message(STATUS "limit 0 exceeded as expected: ${pairs} new-old pairs, counted in ${cpuSeconds} s")

# ---- run 2: the limit equals the count: not exceeded ---------------------------
file(READ "${historyConfig}" configText)
string(REGEX REPLACE "newOldContactLimit = 0" "newOldContactLimit = ${pairs}" atLimitText "${configText}")
if(atLimitText STREQUAL configText)
  message(FATAL_ERROR "CONFIG must hold 'newOldContactLimit = 0'")
endif()
set(atLimitConfig "${WORK_DIR}/history-at-limit.cfg")
file(WRITE "${atLimitConfig}" "${atLimitText}")
set(atLimitOutput "${WORK_DIR}/out-at-limit")
execute_process(
  COMMAND "${HISTORY}" --config "${atLimitConfig}" --output "${atLimitOutput}"
  RESULT_VARIABLE result
  OUTPUT_VARIABLE out
  ERROR_VARIABLE err)
file(WRITE "${WORK_DIR}/history-at-limit.log" "${out}\n--- stderr ---\n${err}")
if(NOT result EQUAL 0)
  message(FATAL_ERROR "limit = pairs (${pairs}) must pass, exit status ${result}:\n${err}\n${out}")
endif()
file(READ "${atLimitOutput}/ledger.json" atLimitLedger)
ledger_value(allPassedAtLimit "${atLimitLedger}" checks allPassed)
ledger_value(pairsAtLimit "${atLimitLedger}" newOldContacts pairs)
if(NOT allPassedAtLimit STREQUAL "ON" OR NOT pairsAtLimit EQUAL pairs)
  message(FATAL_ERROR "at the limit: allPassed=${allPassedAtLimit} pairs=${pairsAtLimit}, expected ON and ${pairs}")
endif()
message(STATUS "limit = ${pairs}: passes with the same ${pairsAtLimit} pairs")
