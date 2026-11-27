# RecordSmoke: run phsp-record on MACRO with 1 and then 2 threads and the same
# SEED, require byte-identical .phsp and .json files, read the file back with
# phsp-dump, check the header counts against the history table, and report the
# run figures.
#   -DRECORD=<phsp-record> -DDUMP=<phsp-dump> -DMACRO=<file> -DOUTPUT_DIR=<dir> -DSEED=<s>
foreach(var RECORD DUMP MACRO OUTPUT_DIR SEED)
  if(NOT DEFINED ${var})
    message(FATAL_ERROR "RunRecordSmoke.cmake: ${var} is not set")
  endif()
endforeach()

file(REMOVE_RECURSE "${OUTPUT_DIR}")
file(MAKE_DIRECTORY "${OUTPUT_DIR}")

# Runs phsp-record with `threads` threads into OUTPUT_DIR/t<threads>/smoke and
# sets <outVar> to its RECORD_DONE line.
function(run_record threads outVar)
  execute_process(
    COMMAND "${RECORD}" "${MACRO}" --threads ${threads} --seed ${SEED} --output "${OUTPUT_DIR}/t${threads}/smoke"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE out
    ERROR_VARIABLE err)
  file(WRITE "${OUTPUT_DIR}/t${threads}.log" "${out}\n--- stderr ---\n${err}")
  if(NOT result EQUAL 0)
    message(FATAL_ERROR "phsp-record --threads ${threads} failed (${result}):\n${err}\n(log: ${OUTPUT_DIR}/t${threads}.log)")
  endif()
  string(REGEX MATCH "RECORD_DONE[^\r\n]*" done "${out}")
  if(done STREQUAL "")
    message(FATAL_ERROR "phsp-record --threads ${threads} printed no RECORD_DONE line (log: ${OUTPUT_DIR}/t${threads}.log)")
  endif()
  set(${outVar} "${done}" PARENT_SCOPE)
endfunction()

# Value of `key=` in a RECORD_DONE line.
function(done_value line key outVar)
  string(REGEX MATCH " ${key}=([^ ]+)" match "${line}")
  if(match STREQUAL "")
    message(FATAL_ERROR "no ${key}= in: ${line}")
  endif()
  set(${outVar} "${CMAKE_MATCH_1}" PARENT_SCOPE)
endfunction()

run_record(1 done1)
run_record(2 done2)
message(STATUS "1 thread : ${done1}")
message(STATUS "2 threads: ${done2}")

foreach(ext phsp json)
  set(a "${OUTPUT_DIR}/t1/smoke.${ext}")
  set(b "${OUTPUT_DIR}/t2/smoke.${ext}")
  foreach(f "${a}" "${b}")
    if(NOT EXISTS "${f}")
      message(FATAL_ERROR "phsp-record did not write ${f}")
    endif()
  endforeach()
  execute_process(COMMAND ${CMAKE_COMMAND} -E compare_files "${a}" "${b}" RESULT_VARIABLE differ)
  if(NOT differ EQUAL 0)
    message(FATAL_ERROR "smoke.${ext} differs between --threads 1 and --threads 2 (same seed)")
  endif()
  message(STATUS "smoke.${ext}: identical for --threads 1 and --threads 2")
endforeach()

# ---- Read the file back -----------------------------------------------------------
set(phsp "${OUTPUT_DIR}/t1/smoke.phsp")
execute_process(COMMAND "${DUMP}" "${phsp}" --section header
  RESULT_VARIABLE dumpResult OUTPUT_VARIABLE header ERROR_VARIABLE dumpErr)
if(NOT dumpResult EQUAL 0)
  message(FATAL_ERROR "phsp-dump --section header failed (${dumpResult}): ${dumpErr}")
endif()
foreach(field primariesFired historiesWritten recordsWritten stage1Seed)
  string(REGEX MATCH "(^|\n)${field},([0-9]+)" match "${header}")
  if(match STREQUAL "")
    message(FATAL_ERROR "phsp-dump header has no ${field}:\n${header}")
  endif()
  set(${field} "${CMAKE_MATCH_2}")
endforeach()
if(NOT stage1Seed STREQUAL "${SEED}")
  message(FATAL_ERROR "header stage1Seed ${stage1Seed} != --seed ${SEED}")
endif()

execute_process(COMMAND "${DUMP}" "${phsp}" --section histories
  RESULT_VARIABLE dumpResult OUTPUT_VARIABLE table ERROR_VARIABLE dumpErr)
if(NOT dumpResult EQUAL 0)
  message(FATAL_ERROR "phsp-dump --section histories failed (${dumpResult}): ${dumpErr}")
endif()
# Rows: historyIndex,stage1EventId,boxIx,boxIy,firstRecord,recordCount,inBoxDepositEv
string(REGEX MATCHALL "\n[0-9]+,[0-9]+,-?[0-9]+,-?[0-9]+,[0-9]+,[0-9]+," rows "${table}")
set(recordSum 0)
set(rowCount 0)
set(eventIds "")
foreach(row IN LISTS rows)
  string(REGEX MATCH "\n[0-9]+,([0-9]+),-?[0-9]+,-?[0-9]+,[0-9]+,([0-9]+)," match "${row}")
  list(APPEND eventIds ${CMAKE_MATCH_1})
  math(EXPR recordSum "${recordSum} + ${CMAKE_MATCH_2}")
  math(EXPR rowCount "${rowCount} + 1")
endforeach()
# One history per (primary, box): distinct event IDs = primaries with a record.
list(REMOVE_DUPLICATES eventIds)
list(LENGTH eventIds primariesWithRecords)

execute_process(COMMAND "${DUMP}" "${phsp}" --section records
  RESULT_VARIABLE dumpResult OUTPUT_QUIET ERROR_VARIABLE dumpErr)
if(NOT dumpResult EQUAL 0)
  message(FATAL_ERROR "phsp-dump --section records failed (${dumpResult}): ${dumpErr}")
endif()

if(historiesWritten EQUAL 0)
  message(FATAL_ERROR "no history written: the smoke geometry must give box crossings")
endif()
if(NOT rowCount EQUAL historiesWritten)
  message(FATAL_ERROR "history table has ${rowCount} rows, header historiesWritten = ${historiesWritten}")
endif()
if(NOT recordSum EQUAL recordsWritten)
  message(FATAL_ERROR "sum of history record counts ${recordSum} != header recordsWritten ${recordsWritten}")
endif()
# histories <= primaries holds for a single box only: on a lattice one primary
# may give a history in each box it enters. The primaries with a record never
# exceed the primaries fired.
if(primariesWithRecords GREATER primariesFired)
  message(FATAL_ERROR "${primariesWithRecords} primaries with records > primariesFired ${primariesFired}")
endif()
done_value("${done1}" primaries donePrimaries)
if(NOT donePrimaries EQUAL primariesFired)
  message(FATAL_ERROR "RECORD_DONE primaries=${donePrimaries} != header primariesFired ${primariesFired}")
endif()
done_value("${done1}" primariesWithRecords donePrimariesWithRecords)
if(NOT donePrimariesWithRecords EQUAL primariesWithRecords)
  message(FATAL_ERROR "RECORD_DONE primariesWithRecords=${donePrimariesWithRecords} != ${primariesWithRecords} "
                      "distinct event IDs in the history table")
endif()

done_value("${done1}" meanRecordsPerHistory meanRecords)
done_value("${done1}" hitFraction hitFraction)
done_value("${done1}" meanInBoxDepositPerHistoryEv meanDepositHistory)
done_value("${done1}" meanInBoxDepositPerPrimaryEv meanDepositPrimary)
done_value("${done1}" timePerPrimaryMs time1)
done_value("${done2}" timePerPrimaryMs time2)
message(STATUS "header consistent: records ${recordsWritten} = sum of history record counts, "
               "primaries with records ${primariesWithRecords} <= primaries ${primariesFired}")
message(STATUS "primaries fired ${primariesFired}, primaries with records ${primariesWithRecords}, "
               "histories written ${historiesWritten}, records written ${recordsWritten}")
message(STATUS "mean records per history ${meanRecords}, hit fraction ${hitFraction}")
message(STATUS "mean in-box deposit ${meanDepositHistory} eV per history, ${meanDepositPrimary} eV per primary")
message(STATUS "time per primary (thread time): ${time1} ms with 1 thread, ${time2} ms with 2 threads")
