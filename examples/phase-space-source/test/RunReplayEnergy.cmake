# ReplayEnergy: replay every history of the phase-space file PHSP once,
# physics only (/phsp/chem/enable false), and compare, over the same histories, the stage-2 mean deposit per history with
# the stage-1 in-box deposit per history (the history table). Both are the
# energy deposited in steps whose pre-step point is in the box. The test is
# on the per-history difference d = stage 2 - stage 1 (paired: both stages see
# the same records): z = mean(d) / SE(d) must satisfy |z| < 3. Prints both
# means, their standard errors and z.
#   -DREPLAY=<phsp-replay> -DDUMP=<phsp-dump> -DPHSP=<file.phsp> -DOUTPUT_DIR=<dir> -DBASE_SEED=<s>
foreach(var REPLAY DUMP PHSP OUTPUT_DIR BASE_SEED)
  if(NOT DEFINED ${var})
    message(FATAL_ERROR "RunReplayEnergy.cmake: ${var} is not set")
  endif()
endforeach()
include(${CMAKE_CURRENT_LIST_DIR}/ReplayTestUtilities.cmake)

file(REMOVE_RECURSE "${OUTPUT_DIR}")
file(MAKE_DIRECTORY "${OUTPUT_DIR}")

phsp_histories_written("${PHSP}" historyCount)
if(historyCount LESS 2)
  message(FATAL_ERROR "${PHSP} holds ${historyCount} histories; a standard error needs at least 2")
endif()
phsp_write_replay_macro("${OUTPUT_DIR}/full.mac" "${PHSP}" ${BASE_SEED} "/phsp/chem/enable false" "0 ${historyCount}")
phsp_run_replay(energy "${OUTPUT_DIR}/full.mac" 1 done)
message(STATUS "${done}")

phsp_done_value("${done}" histories histories)
if(NOT histories EQUAL historyCount)
  message(FATAL_ERROR "replayed ${histories} histories, the file holds ${historyCount}")
endif()
foreach(key stage1MeanEv stage1SeEv stage2MeanEv stage2SeEv diffMeanEv diffSeEv z tracksKilledOutsideBox)
  phsp_done_value("${done}" ${key} ${key})
endforeach()
message(STATUS "stage-1 in-box deposit per history: ${stage1MeanEv} +/- ${stage1SeEv} eV (mean +/- SE, ${histories} histories)")
message(STATUS "stage-2 deposit per history:        ${stage2MeanEv} +/- ${stage2SeEv} eV")
message(STATUS "stage 2 - stage 1 per history:      ${diffMeanEv} +/- ${diffSeEv} eV, z = ${z}")
message(STATUS "tracks found outside the box and killed: ${tracksKilledOutsideBox}")
if(NOT tracksKilledOutsideBox EQUAL 0)
  # Every primary starts on the box surface, entering it: a track outside the
  # box means a primary was located in the world (its deposit is lost).
  message(FATAL_ERROR "${tracksKilledOutsideBox} tracks were found outside the box; expected 0")
endif()
if(NOT (z GREATER -3 AND z LESS 3))
  message(FATAL_ERROR "energy check failed: |z| = |${z}| >= 3")
endif()
message(STATUS "ReplayEnergy: |z| < 3")
