# Stock build: the start-trial gate (include/StartTrialGate.hh) on a one-packet history.
#   -DHISTORY=<exe> -DWORK_DIR=<dir> -DLIBRARY=<dir> -DCONFIG=<lines> -DSCHEDULE=<rows>
#       (inputs: see HistoryTestInput.cmake; one source at the start time)
# The history runs twice with the same config. Each run must report, in
# ledger.json firstPacketSecondTrial, a packet whose start-time trial matters
# (pairs > 0, expectedReactions > 0), the gate on (decision "gated",
# gatedReactionData > 0) and no reaction in a zero time step at the start time
# (startZeroStepReactions 0). species.csv and activations.csv must be
# byte-identical between the two runs.
include("${CMAKE_CURRENT_LIST_DIR}/HistoryTestInput.cmake")

function(run_history outputDir tag)
  execute_process(
    COMMAND "${HISTORY}" --config "${historyConfig}" --output "${outputDir}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE out
    ERROR_VARIABLE err)
  file(WRITE "${WORK_DIR}/history-${tag}.log" "${out}\n--- stderr ---\n${err}")
  if(NOT result EQUAL 0)
    message(FATAL_ERROR "history (${tag}) failed (${result}):\n${err}\n${out}")
  endif()
  if(NOT out MATCHES "HISTORY_DONE[^\n]* sources=1 ")
    message(FATAL_ERROR "history (${tag}) did not finish one source:\n${out}")
  endif()
endfunction()

function(check_ledger outputDir tag)
  file(READ "${outputDir}/ledger.json" ledger)
  string(JSON buildMode GET "${ledger}" buildMode)
  if(NOT buildMode STREQUAL "stock")
    message(FATAL_ERROR "ledger.json (${tag}) buildMode is '${buildMode}': this test is for the stock build")
  endif()
  foreach(field packetIndex pairs expectedReactions decision gatedReactionData startZeroStepReactions)
    string(JSON ${field} ERROR_VARIABLE problem GET "${ledger}" firstPacketSecondTrial ${field})
    if(NOT problem STREQUAL "NOTFOUND")
      message(FATAL_ERROR "ledger.json (${tag}) lacks firstPacketSecondTrial.${field}: ${problem}")
    endif()
  endforeach()
  if(NOT pairs GREATER 0 OR NOT expectedReactions GREATER 0)
    message(FATAL_ERROR "the fixture packet has no start-time contact (pairs ${pairs}, "
                        "expectedReactions ${expectedReactions}): the test would check nothing")
  endif()
  if(NOT decision STREQUAL "gated" OR NOT gatedReactionData GREATER 0)
    message(FATAL_ERROR "the start-trial gate is off (${tag}): decision '${decision}', "
                        "gatedReactionData ${gatedReactionData}")
  endif()
  if(NOT startZeroStepReactions EQUAL 0)
    message(FATAL_ERROR "${startZeroStepReactions} reactions in a zero time step at the start time (${tag}), "
                        "expected 0 with the gate on (expectedReactions without it: ${expectedReactions})")
  endif()
  message(STATUS "${tag}: packet ${packetIndex}, ${pairs} pairs, sum p ${expectedReactions}, "
                 "${gatedReactionData} reaction data gated, ${startZeroStepReactions} zero-step reactions at start")
endfunction()

run_history("${historyOutput}" first)
run_history("${WORK_DIR}/repeat" repeat)
check_ledger("${historyOutput}" first)
check_ledger("${WORK_DIR}/repeat" repeat)
foreach(file species.csv activations.csv)
  file(SHA256 "${historyOutput}/${file}" firstHash)
  file(SHA256 "${WORK_DIR}/repeat/${file}" repeatHash)
  if(NOT firstHash STREQUAL repeatHash)
    message(FATAL_ERROR "${file} differs between two runs of the same config")
  endif()
endforeach()
message(STATUS "species.csv and activations.csv are byte-identical on repeat")
