# CaptureSeedReproducible: a packet depends only on its declared physics seed.
# Packet 1 of a run with --first-seed S must be byte-identical to packet 0 of a
# run with --first-seed S+1, and to packet 1 of a repeated run.
#   -DCAPTURE=<exe> -DMACRO=<file> -DOUTPUT_DIR=<dir> -DFIRST_SEED=<s>
foreach(var CAPTURE MACRO OUTPUT_DIR FIRST_SEED)
  if(NOT DEFINED ${var})
    message(FATAL_ERROR "RunCaptureSeedCheck.cmake: ${var} is not set")
  endif()
endforeach()

math(EXPR nextSeed "${FIRST_SEED} + 1")
file(REMOVE_RECURSE "${OUTPUT_DIR}")
foreach(run "pair-a;${FIRST_SEED};2" "pair-b;${FIRST_SEED};2" "single;${nextSeed};1")
  list(GET run 0 name)
  list(GET run 1 seed)
  list(GET run 2 count)
  execute_process(
    COMMAND "${CAPTURE}" --macro "${MACRO}" --output "${OUTPUT_DIR}/${name}" --count ${count} --first-seed ${seed}
    RESULT_VARIABLE result
    OUTPUT_VARIABLE out
    ERROR_VARIABLE err)
  if(NOT result EQUAL 0)
    message(FATAL_ERROR "capture ${name} failed (${result}):\n${err}")
  endif()
endforeach()

file(SHA256 "${OUTPUT_DIR}/pair-a/packet-00000.txt" a0)
file(SHA256 "${OUTPUT_DIR}/pair-b/packet-00000.txt" b0)
file(SHA256 "${OUTPUT_DIR}/pair-a/packet-00001.txt" a1)
file(SHA256 "${OUTPUT_DIR}/pair-b/packet-00001.txt" b1)
file(SHA256 "${OUTPUT_DIR}/single/packet-00000.txt" s0)
message(STATUS "seed ${FIRST_SEED}: ${a0} / ${b0}")
message(STATUS "seed ${nextSeed}: ${a1} / ${b1} / single ${s0}")
if(NOT a0 STREQUAL b0 OR NOT a1 STREQUAL b1)
  message(FATAL_ERROR "a repeated capture with the same seeds wrote different packets")
endif()
if(NOT a1 STREQUAL s0)
  message(FATAL_ERROR "packet of seed ${nextSeed} depends on the event before it")
endif()
if(a0 STREQUAL a1)
  message(FATAL_ERROR "different seeds gave identical packets")
endif()
