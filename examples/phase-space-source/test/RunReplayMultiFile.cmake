# ReplayMultiFile: a replay over two files, physics only (/phsp/chem/enable false).
#   a  files [PHSP, SECOND]: replay the histories of SECOND alone, global
#      indices N..N+M-1 (N = histories of PHSP, M = histories of SECOND)
#   b  files [SECOND]: histories 0..M-1
#   c  files [OTHER_ID]: the same content as SECOND under another file ID
#      (another stage-1 seed), histories 0..M-1
# and checks
#   - a's rows carry SECOND's file ID, history indices 0..M-1 and global
#     indices N..N+M-1, and a's Manifest.json lists both files in order (file
#     IDs, firstGlobalIndex 0 and N) and the run range (first N, count M);
#   - b's rows equal a's in every column but globalIndex: the per-history seed
#     (ADR 0015) comes from (base seed, file ID, history index), not from the
#     global index or the position of the file in the list;
#   - c keeps every stage-1 column of b and changes at least one stage-2
#     deposit: the file ID is in the seed.
#   -DREPLAY=<phsp-replay> -DDUMP=<phsp-dump> -DPHSP=<first file> -DSECOND=<second file>
#   -DOTHER_ID=<copy of SECOND's content, other ID> -DOUTPUT_DIR=<dir> -DBASE_SEED=<s>
foreach(var REPLAY DUMP PHSP SECOND OTHER_ID OUTPUT_DIR BASE_SEED)
  if(NOT DEFINED ${var})
    message(FATAL_ERROR "RunReplayMultiFile.cmake: ${var} is not set")
  endif()
endforeach()
include(${CMAKE_CURRENT_LIST_DIR}/ReplayTestUtilities.cmake)

file(REMOVE_RECURSE "${OUTPUT_DIR}")
file(MAKE_DIRECTORY "${OUTPUT_DIR}")

phsp_histories_written("${PHSP}" firstCount)
phsp_histories_written("${SECOND}" secondCount)
if(secondCount LESS 2)
  message(FATAL_ERROR "${SECOND} holds ${secondCount} histories; expected at least 2")
endif()

# fileId of a phase-space file, read with phsp-dump.
function(file_id phsp outVar)
  execute_process(COMMAND "${DUMP}" "${phsp}" --section header RESULT_VARIABLE result OUTPUT_VARIABLE header)
  string(REGEX MATCH "(^|\n)fileId,(0x[0-9a-f]+)" match "${header}")
  if(NOT result EQUAL 0 OR match STREQUAL "")
    message(FATAL_ERROR "no fileId in the phsp-dump header of ${phsp}")
  endif()
  set(${outVar} "${CMAKE_MATCH_2}" PARENT_SCOPE)
endfunction()
file_id("${PHSP}" firstId)
file_id("${SECOND}" secondId)
file_id("${OTHER_ID}" otherId)
if(secondId STREQUAL otherId)
  message(FATAL_ERROR "${SECOND} and ${OTHER_ID} have the same file ID ${secondId}")
endif()

set(head "/control/verbose 0\n/run/verbose 0\n/process/em/verbose 0\n/phsp/chem/enable false\n")
file(WRITE "${OUTPUT_DIR}/a.mac"
  "${head}/phsp/source/addFile ${PHSP}\n/phsp/source/addFile ${SECOND}\n/phsp/source/baseSeed ${BASE_SEED}\n"
  "/run/initialize\n/phsp/source/first ${firstCount}\n/run/beamOn ${secondCount}\n")
file(WRITE "${OUTPUT_DIR}/b.mac"
  "${head}/phsp/source/addFile ${SECOND}\n/phsp/source/baseSeed ${BASE_SEED}\n/run/initialize\n/run/beamOn ${secondCount}\n")
file(WRITE "${OUTPUT_DIR}/c.mac"
  "${head}/phsp/source/addFile ${OTHER_ID}\n/phsp/source/baseSeed ${BASE_SEED}\n/run/initialize\n/run/beamOn ${secondCount}\n")
phsp_run_replay(a "${OUTPUT_DIR}/a.mac" 1 doneA)
phsp_run_replay(b "${OUTPUT_DIR}/b.mac" 1 doneB)
phsp_run_replay(c "${OUTPUT_DIR}/c.mac" 1 doneC)
message(STATUS "a: ${doneA}")
message(STATUS "b: ${doneB}")
message(STATUS "c: ${doneC}")

file(STRINGS "${OUTPUT_DIR}/a/Histories.csv" rowsA)
file(STRINGS "${OUTPUT_DIR}/b/Histories.csv" rowsB)
file(STRINGS "${OUTPUT_DIR}/c/Histories.csv" rowsC)
math(EXPR expectedLines "${secondCount} + 1")
foreach(run A B C)
  list(LENGTH rows${run} lines)
  if(NOT lines EQUAL expectedLines)
    message(FATAL_ERROR "run ${run}: ${lines} lines in Histories.csv, expected ${expectedLines}")
  endif()
endforeach()

set(stage2Differing 0)
foreach(i RANGE 1 ${secondCount})
  list(GET rowsA ${i} rowA)
  list(GET rowsB ${i} rowB)
  list(GET rowsC ${i} rowC)
  string(REPLACE "," ";" fieldsA "${rowA}")
  string(REPLACE "," ";" fieldsB "${rowB}")
  string(REPLACE "," ";" fieldsC "${rowC}")
  math(EXPR historyIndex "${i} - 1")
  math(EXPR globalIndex "${firstCount} + ${historyIndex}")
  list(GET fieldsA 0 globalA)
  list(GET fieldsA 1 idA)
  list(GET fieldsA 2 indexA)
  if(NOT globalA EQUAL globalIndex OR NOT idA STREQUAL secondId OR NOT indexA EQUAL historyIndex)
    message(FATAL_ERROR "run a row ${i}: ${rowA}\nexpected globalIndex ${globalIndex}, fileId ${secondId}, "
                        "historyIndex ${historyIndex}")
  endif()
  # Every column but globalIndex: the same history gets the same physics.
  list(SUBLIST fieldsA 1 -1 tailA)
  list(SUBLIST fieldsB 1 -1 tailB)
  if(NOT tailA STREQUAL tailB)
    message(FATAL_ERROR "history ${historyIndex} of ${SECOND} differs between its global index ${globalIndex} "
                        "(run a, second file) and 0-based (run b, only file):\n  a: ${rowA}\n  b: ${rowB}")
  endif()
  # Run c: columns 3..7 (stage1EventId .. stage1InBoxDepositEv) as b; column 8 = stage2DepositEv.
  list(SUBLIST fieldsB 3 5 stage1B)
  list(SUBLIST fieldsC 3 5 stage1C)
  list(GET fieldsC 1 idC)
  if(NOT stage1B STREQUAL stage1C OR NOT idC STREQUAL otherId)
    message(FATAL_ERROR "run c row ${i}: ${rowC}\nexpected file ID ${otherId} and the stage-1 columns of b: ${rowB}")
  endif()
  list(GET fieldsB 8 depositB)
  list(GET fieldsC 8 depositC)
  if(NOT depositB STREQUAL depositC)
    math(EXPR stage2Differing "${stage2Differing} + 1")
  endif()
endforeach()
message(STATUS "run a (second file, global ${firstCount}..) vs run b (only file): identical rows but globalIndex, "
               "${secondCount} histories")
message(STATUS "run c (same content, file ID ${otherId}) vs run b (file ID ${secondId}): "
               "${stage2Differing} of ${secondCount} stage-2 deposits differ")
if(stage2Differing EQUAL 0)
  message(FATAL_ERROR "another file ID changed no stage-2 deposit: the file ID is not in the per-history seed")
endif()

# Manifest.json of run a: both files in order, their first global indices, the range.
file(READ "${OUTPUT_DIR}/a/Manifest.json" manifest)
# Fails unless the manifest matches `pattern` (one check at a time: the
# patterns hold brackets, which a CMake list would mangle).
function(require_manifest pattern what)
  if(NOT manifest MATCHES "${pattern}")
    message(FATAL_ERROR "run a: Manifest.json does not record ${what}")
  endif()
endfunction()
require_manifest("\"position\": 0,[^}]*\"fileId\": \"${firstId}\"[^}]*\"firstGlobalIndex\": 0,[^}]*}[^{]*{[^}]*\"position\": 1,[^}]*\"fileId\": \"${secondId}\"[^}]*\"firstGlobalIndex\": ${firstCount},"
                 "files: positions 0 and 1 with their file IDs and first global indices 0 and ${firstCount}")
math(EXPR totalHistories "${firstCount} + ${secondCount}")
require_manifest("\"historiesInFiles\": ${totalHistories}," "historiesInFiles ${totalHistories}")
require_manifest("\"first\": ${firstCount},[^}]*\"count\": ${secondCount}" "the run range: first ${firstCount}, count ${secondCount}")
require_manifest("\"baseSeed\": \"${BASE_SEED}\"" "baseSeed ${BASE_SEED}")
require_manifest("\"historiesReplayed\": ${secondCount}," "historiesReplayed ${secondCount}")
require_manifest("\"enabled\": false" "chemistry enabled false")
message(STATUS "Manifest.json of run a: both files, their IDs and first global indices, the range, the base seed")
message(STATUS "ReplayMultiFile: passed")
