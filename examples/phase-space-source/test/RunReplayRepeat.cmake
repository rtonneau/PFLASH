# ReplayRepeat: replay the histories of the phase-space file PHSP with
# phsp-replay seven times and compare the Histories.csv rows:
#   a  the whole range in one run, 1 thread
#   b  the same again (a repeat)
#   c  the same range split into two runs (first 0 + K, first K + N - K) in one process
#   d  the whole range on 2 threads
#   f  the whole range with another run seed (/random/setSeeds), same base seed
#   g  the second part alone (first K, N - K histories) in a fresh process
#   e  the whole range with base seed BASE_SEED + 1
# a, b, c, d and f must be identical, and g must equal rows K..N-1 of a.
#
# What each run shows. G4MTRunManager draws the per-event seeds from the
# master engine in event-ID order, so b, c and d would match a even WITHOUT the
# per-history reseed (c's second run continues the master's seed stream where
# its first run stopped). Only f and g need the reseed of ADR 0015: f changes
# every per-event seed the run manager hands out, and g gives history K the
# seeds of event 0 of a fresh master stream. e must differ in at least one
# stage-2 deposit (the base seed is in effect) and agree in every stage-1
# column.
#   -DREPLAY=<phsp-replay> -DDUMP=<phsp-dump> -DPHSP=<file.phsp> -DOUTPUT_DIR=<dir> -DBASE_SEED=<s>
foreach(var REPLAY DUMP PHSP OUTPUT_DIR BASE_SEED)
  if(NOT DEFINED ${var})
    message(FATAL_ERROR "RunReplayRepeat.cmake: ${var} is not set")
  endif()
endforeach()
include(${CMAKE_CURRENT_LIST_DIR}/ReplayTestUtilities.cmake)

file(REMOVE_RECURSE "${OUTPUT_DIR}")
file(MAKE_DIRECTORY "${OUTPUT_DIR}")

phsp_histories_written("${PHSP}" historyCount)
if(historyCount LESS 2)
  message(FATAL_ERROR "${PHSP} holds ${historyCount} histories; the split run needs at least 2")
endif()
math(EXPR splitAt "${historyCount} / 2")
math(EXPR secondCount "${historyCount} - ${splitAt}")
math(EXPR otherSeed "${BASE_SEED} + 1")

phsp_write_replay_macro("${OUTPUT_DIR}/full.mac" "${PHSP}" ${BASE_SEED} "" "0 ${historyCount}")
phsp_write_replay_macro("${OUTPUT_DIR}/split.mac" "${PHSP}" ${BASE_SEED} "" "0 ${splitAt}" "${splitAt} ${secondCount}")
phsp_write_replay_macro("${OUTPUT_DIR}/run-seeds.mac" "${PHSP}" ${BASE_SEED} "/random/setSeeds 987654 321" "0 ${historyCount}")
phsp_write_replay_macro("${OUTPUT_DIR}/second-part.mac" "${PHSP}" ${BASE_SEED} "" "${splitAt} ${secondCount}")
phsp_write_replay_macro("${OUTPUT_DIR}/other-seed.mac" "${PHSP}" ${otherSeed} "" "0 ${historyCount}")

phsp_run_replay(a "${OUTPUT_DIR}/full.mac" 1 doneA)
phsp_run_replay(b "${OUTPUT_DIR}/full.mac" 1 doneB)
phsp_run_replay(c "${OUTPUT_DIR}/split.mac" 1 doneC)
phsp_run_replay(d "${OUTPUT_DIR}/full.mac" 2 doneD)
phsp_run_replay(f "${OUTPUT_DIR}/run-seeds.mac" 1 doneF)
phsp_run_replay(g "${OUTPUT_DIR}/second-part.mac" 1 doneG)
phsp_run_replay(e "${OUTPUT_DIR}/other-seed.mac" 1 doneE)

# Lines of a Histories.csv file (header + one row per history).
function(read_rows run outVar)
  set(csv "${OUTPUT_DIR}/${run}/Histories.csv")
  if(NOT EXISTS "${csv}")
    message(FATAL_ERROR "run ${run} wrote no ${csv}")
  endif()
  file(STRINGS "${csv}" lines)
  set(${outVar} "${lines}" PARENT_SCOPE)
endfunction()

read_rows(a rowsA)
list(LENGTH rowsA linesA)
math(EXPR expectedLines "${historyCount} + 1")
if(NOT linesA EQUAL expectedLines)
  message(FATAL_ERROR "run a: ${linesA} lines in Histories.csv, expected ${expectedLines} (header + ${historyCount} rows)")
endif()

# Number of data rows of run `other` that differ from rows offset+1.. of run
# a (offset = global index of the first row of `other`). The headers must match.
function(count_differing other offset outVar)
  read_rows(${other} rows)
  list(LENGTH rows lines)
  math(EXPR expected "${linesA} - ${offset}")
  if(NOT lines EQUAL expected)
    message(FATAL_ERROR "run ${other}: ${lines} lines in Histories.csv, expected ${expected}")
  endif()
  list(GET rowsA 0 headerA)
  list(GET rows 0 header)
  if(NOT header STREQUAL headerA)
    message(FATAL_ERROR "run ${other}: Histories.csv header differs from run a")
  endif()
  set(differing 0)
  math(EXPR last "${lines} - 1")
  foreach(i RANGE 1 ${last})
    math(EXPR j "${i} + ${offset}")
    list(GET rowsA ${j} rowA)
    list(GET rows ${i} row)
    if(NOT rowA STREQUAL row)
      math(EXPR differing "${differing} + 1")
      if(differing LESS_EQUAL 3)
        message(STATUS "run ${other} row ${i}: ${row}\n          run a row ${j}: ${rowA}")
      endif()
    endif()
  endforeach()
  set(${outVar} ${differing} PARENT_SCOPE)
endfunction()

set(failures "")
foreach(run b c d f)
  count_differing(${run} 0 differing)
  message(STATUS "run ${run} vs run a: ${differing} differing rows of ${historyCount}")
  if(NOT differing EQUAL 0)
    list(APPEND failures "${run}")
  endif()
endforeach()
count_differing(g ${splitAt} differing)
math(EXPR lastIndex "${historyCount} - 1")
message(STATUS "run g (histories ${splitAt}..${lastIndex} alone) vs rows ${splitAt}.. of run a: "
               "${differing} differing rows of ${secondCount}")
if(NOT differing EQUAL 0)
  list(APPEND failures "g")
endif()
if(failures)
  message(FATAL_ERROR "per-history results differ from run a in run(s) ${failures} "
                      "(b = repeat, c = split into two runs, d = 2 threads, f = other /random/setSeeds, "
                      "g = second part alone)")
endif()

# Run e (another base seed): stage-1 columns equal, at least one stage-2 deposit different.
read_rows(e rowsE)
set(stage2Differing 0)
math(EXPR last "${linesA} - 1")
foreach(i RANGE 1 ${last})
  list(GET rowsA ${i} rowA)
  list(GET rowsE ${i} rowE)
  string(REPLACE "," ";" fieldsA "${rowA}")
  string(REPLACE "," ";" fieldsE "${rowE}")
  # Columns 0..7: globalIndex .. stage1InBoxDepositEv; column 8: stage2DepositEv.
  list(SUBLIST fieldsA 0 8 stage1A)
  list(SUBLIST fieldsE 0 8 stage1E)
  if(NOT stage1A STREQUAL stage1E)
    message(FATAL_ERROR "run e (base seed ${otherSeed}) changed the stage-1 columns of row ${i}:\n${rowE}\n${rowA}")
  endif()
  list(GET fieldsA 8 depositA)
  list(GET fieldsE 8 depositE)
  if(NOT depositA STREQUAL depositE)
    math(EXPR stage2Differing "${stage2Differing} + 1")
  endif()
endforeach()
message(STATUS "run e (base seed ${otherSeed}) vs run a (base seed ${BASE_SEED}): "
               "${stage2Differing} of ${historyCount} stage-2 deposits differ")
if(stage2Differing EQUAL 0)
  message(FATAL_ERROR "changing the base seed changed no stage-2 deposit: the per-history seed is not in effect")
endif()

message(STATUS "a: ${doneA}")
message(STATUS "d: ${doneD}")
message(STATUS "ReplayRepeat: 0 differing rows across runs a, b, c, d, f and g (${historyCount} histories; "
               "g holds ${secondCount})")
