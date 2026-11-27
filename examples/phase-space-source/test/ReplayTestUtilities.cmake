# Helpers shared by RunReplayRepeat.cmake and RunReplayEnergy.cmake. They use
# the caller's REPLAY, DUMP and OUTPUT_DIR.

# historiesWritten of a phase-space file, read with phsp-dump.
function(phsp_histories_written phsp outVar)
  execute_process(COMMAND "${DUMP}" "${phsp}" --section header
    RESULT_VARIABLE result OUTPUT_VARIABLE header ERROR_VARIABLE err)
  if(NOT result EQUAL 0)
    message(FATAL_ERROR "phsp-dump --section header ${phsp} failed (${result}): ${err}")
  endif()
  string(REGEX MATCH "(^|\n)historiesWritten,([0-9]+)" match "${header}")
  if(match STREQUAL "")
    message(FATAL_ERROR "phsp-dump header of ${phsp} has no historiesWritten:\n${header}")
  endif()
  set(${outVar} "${CMAKE_MATCH_2}" PARENT_SCOPE)
endfunction()

# Writes a phsp-replay macro: one file, a base seed, the commands in `extra`
# (a string, may be empty; placed before /run/initialize), then one
# `/phsp/source/first k` + `/run/beamOn n` pair per "k n" argument.
function(phsp_write_replay_macro path phsp baseSeed extra)
  set(text "/control/verbose 0\n/run/verbose 0\n/process/em/verbose 0\n")
  string(APPEND text "/phsp/source/addFile ${phsp}\n/phsp/source/baseSeed ${baseSeed}\n")
  if(NOT extra STREQUAL "")
    string(APPEND text "${extra}\n")
  endif()
  string(APPEND text "/run/initialize\n")
  foreach(range IN LISTS ARGN)
    separate_arguments(pair UNIX_COMMAND "${range}")
    list(GET pair 0 first)
    list(GET pair 1 count)
    string(APPEND text "/phsp/source/first ${first}\n/run/beamOn ${count}\n")
  endforeach()
  file(WRITE "${path}" "${text}")
endfunction()

# Runs phsp-replay on `macro` with `threads` threads into OUTPUT_DIR/<name> and
# sets <outVar> to its REPLAY_DONE line.
function(phsp_run_replay name macro threads outVar)
  execute_process(
    COMMAND "${REPLAY}" "${macro}" --threads ${threads} --output "${OUTPUT_DIR}/${name}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE out
    ERROR_VARIABLE err)
  file(WRITE "${OUTPUT_DIR}/${name}.log" "${out}\n--- stderr ---\n${err}")
  if(NOT result EQUAL 0)
    message(FATAL_ERROR "phsp-replay run ${name} (--threads ${threads}) failed (${result}):\n${err}\n"
                        "(log: ${OUTPUT_DIR}/${name}.log)")
  endif()
  string(REGEX MATCH "REPLAY_DONE[^\r\n]*" done "${out}")
  if(done STREQUAL "")
    message(FATAL_ERROR "phsp-replay run ${name} printed no REPLAY_DONE line (log: ${OUTPUT_DIR}/${name}.log)")
  endif()
  set(${outVar} "${done}" PARENT_SCOPE)
endfunction()

# Value of `key=` in a REPLAY_DONE line.
function(phsp_done_value line key outVar)
  string(REGEX MATCH " ${key}=([^ ]+)" match "${line}")
  if(match STREQUAL "")
    message(FATAL_ERROR "no ${key}= in: ${line}")
  endif()
  set(${outVar} "${CMAKE_MATCH_1}" PARENT_SCOPE)
endfunction()
