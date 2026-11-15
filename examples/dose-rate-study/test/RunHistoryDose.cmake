# Runs one doseCutoff `history` and checks its outputs.
#   -DHISTORY=<exe> -DWORK_DIR=<dir> -DLIBRARY=<dir> -DCONFIG=<lines>
#       (inputs: see HistoryTestInput.cmake; CONFIG must set mode = doseCutoff)
#   [-DEXPECT_STEADY_STATE=ON]  the steady-state stop must have fired, with
#                               extrapolated rows in species.csv
# The history itself fails on any activation, end or energy check; this script
# checks that it said so, and what it wrote.
include("${CMAKE_CURRENT_LIST_DIR}/HistoryTestInput.cmake")

execute_process(
  COMMAND "${HISTORY}" --config "${historyConfig}" --output "${historyOutput}"
  RESULT_VARIABLE result
  OUTPUT_VARIABLE out
  ERROR_VARIABLE err)
file(WRITE "${WORK_DIR}/history.log" "${out}\n--- stderr ---\n${err}")
if(NOT result EQUAL 0)
  message(FATAL_ERROR "history failed (${result}):\n${err}\n${out}")
endif()
string(REGEX MATCH "HISTORY_DONE[^\n]*" doneLine "${out}")
if(doneLine STREQUAL "")
  message(FATAL_ERROR "history printed no HISTORY_DONE line:\n${out}")
endif()
message(STATUS "${doneLine}")
if(NOT doneLine MATCHES " mode=doseCutoff " OR NOT doneLine MATCHES " wallTime_s=")
  message(FATAL_ERROR "the HISTORY_DONE line is not a doseCutoff one")
endif()

foreach(file species.csv species_age.csv activations.csv ledger.json)
  if(NOT EXISTS "${historyOutput}/${file}")
    message(FATAL_ERROR "history did not write ${file}")
  endif()
endforeach()

# ledger.json: the activation, end and energy checks passed.
file(READ "${historyOutput}/ledger.json" ledger)
set(ledgerChecks "\"allPassed\": true" "\"chemistryRan\": true" "\"allLiveAtEndTime\": true"
                 "\"counterMatchesLive\": true" "\"allInsideBox\": true" "\"targetReached\": true"
                 "\"overshootBelowOneTrack\": true" "\"beamOffHoldsEverySource\": true"
                 "\"injectedMatchesSchedule\": true" "\"hitFraction\": " "\"meanDepositPerLaunched_eV\": "
                 "\"launchedRate_Hz\": " "\"packetRate_Hz\": ")
if(EXPECT_STEADY_STATE)
  list(APPEND ledgerChecks "\"reached\": true" "\"stoppedEarly\": true")
else()
  list(APPEND ledgerChecks "\"stoppedEarly\": false")
endif()
foreach(check ${ledgerChecks})
  string(FIND "${ledger}" "${check}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "ledger.json lacks ${check}")
  endif()
endforeach()

# activations.csv: observed activation time = expected for every activated
# source (a source after a steady-state stop is never observed: empty field).
file(STRINGS "${historyOutput}/activations.csv" activationLines)
list(POP_FRONT activationLines activationHeader)
list(LENGTH activationLines sourceCount)
string(REGEX MATCH "\"arrivalCount\": ([0-9]+)" arrivalMatch "${ledger}")
if(NOT CMAKE_MATCH_1 EQUAL sourceCount)
  message(FATAL_ERROR "activations.csv has ${sourceCount} rows, ledger.json arrivalCount '${CMAKE_MATCH_1}'")
endif()
set(observedCount 0)
foreach(line IN LISTS activationLines)
  string(REPLACE "," ";" fields "${line}")
  list(GET fields 6 expected)
  list(GET fields 7 observed)
  if(NOT observed STREQUAL "")
    if(NOT observed STREQUAL expected)
      message(FATAL_ERROR "activation observed at '${observed}', expected '${expected}': ${line}")
    endif()
    math(EXPR observedCount "${observedCount} + 1")
  elseif(NOT EXPECT_STEADY_STATE)
    message(FATAL_ERROR "a source was never observed: ${line}")
  endif()
endforeach()

# species.csv: the dose header; the last row is at beam-off; extrapolated rows
# exist exactly when the steady state was reached.
file(STRINGS "${historyOutput}/species.csv" lines ENCODING UTF-8)
list(POP_FRONT lines header)
set(expectedHeader "timeNs,species,count,energyDepositEv,doseGy,gCumulativePer100eV,gFinalPer100eV,extrapolated")
if(NOT header STREQUAL expectedHeader)
  message(FATAL_ERROR "species.csv: unexpected header '${header}'")
endif()
set(extrapolatedRows 0)
foreach(line IN LISTS lines)
  if(line MATCHES ",1$")
    math(EXPR extrapolatedRows "${extrapolatedRows} + 1")
  elseif(NOT line MATCHES ",0$")
    message(FATAL_ERROR "species.csv: no extrapolated flag: ${line}")
  endif()
endforeach()
if(EXPECT_STEADY_STATE AND extrapolatedRows EQUAL 0)
  message(FATAL_ERROR "species.csv has no extrapolated row after a steady-state stop")
endif()
if(NOT EXPECT_STEADY_STATE AND NOT extrapolatedRows EQUAL 0)
  message(FATAL_ERROR "species.csv has ${extrapolatedRows} extrapolated rows without a steady-state stop")
endif()
list(GET lines -1 lastLine)
string(REGEX REPLACE ",.*" "" lastTime "${lastLine}")
string(REGEX MATCH "\"beamOff_ns\": ([0-9.eE+-]+)" beamOffMatch "${ledger}")
# Both files write reals with 17 significant digits.
if(NOT lastTime STREQUAL CMAKE_MATCH_1)
  message(FATAL_ERROR "the last species.csv time is ${lastTime}, beam-off is '${CMAKE_MATCH_1}'")
endif()

file(STRINGS "${historyOutput}/species_age.csv" ageLines ENCODING UTF-8)
list(GET ageLines 0 ageHeader)
if(NOT ageHeader STREQUAL
   "arrivalIndex,ageNs,timeNs,species,count,energyDepositEv,doseGy,gCumulativePer100eV,gFinalPer100eV")
  message(FATAL_ERROR "species_age.csv: unexpected header '${ageHeader}'")
endif()

string(REGEX MATCH " molecules=([0-9]+)" moleculeMatch "${doneLine}")
message(STATUS "doseCutoff: ${sourceCount} sources (${observedCount} observed), ${CMAKE_MATCH_1} molecules, "
               "${extrapolatedRows} extrapolated rows")
