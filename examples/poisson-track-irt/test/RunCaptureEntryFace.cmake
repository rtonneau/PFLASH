# CaptureEntryFace: with /ptirt/capture/entryFace true every packet starts on the -z face
# of the capture box, and a second capture with the same seeds reproduces the origins.
#   -DCAPTURE=<exe> -DMACRO=<file> -DOUTPUT_DIR=<dir> -DCOUNT=<n> -DFIRST_SEED=<s>
#   -DHALF_SIDE_NM=<whole number of nm, the macro's /ptirt/capture/halfSide>
foreach(var CAPTURE MACRO OUTPUT_DIR COUNT FIRST_SEED HALF_SIDE_NM)
  if(NOT DEFINED ${var})
    message(FATAL_ERROR "RunCaptureEntryFace.cmake: ${var} is not set")
  endif()
endforeach()

file(REMOVE_RECURSE "${OUTPUT_DIR}")
foreach(name first second)
  execute_process(
    COMMAND "${CAPTURE}" --macro "${MACRO}" --output "${OUTPUT_DIR}/${name}" --count ${COUNT} --first-seed ${FIRST_SEED}
    RESULT_VARIABLE result
    OUTPUT_VARIABLE out
    ERROR_VARIABLE err)
  if(NOT result EQUAL 0)
    message(FATAL_ERROR "capture ${name} failed (${result}):\n${err}\n${out}")
  endif()
endforeach()

# Packet origins (exact text) and half side from capture.json.
function(read_origins name outOrigins outHalfSide)
  file(READ "${OUTPUT_DIR}/${name}/capture.json" json)
  string(REGEX MATCH "\"captureHalfSide_nm\": *([0-9.eE+-]+)" ignored "${json}")
  set(${outHalfSide} "${CMAKE_MATCH_1}" PARENT_SCOPE)
  string(REGEX MATCHALL "\"primaryOrigin_nm\": *\\[[0-9., eE+-]*\\]" found "${json}")
  set(${outOrigins} "${found}" PARENT_SCOPE)
endfunction()

read_origins(first originsA halfSide)
read_origins(second originsB halfSideB)
list(LENGTH originsA originCount)
if(NOT originCount EQUAL COUNT)
  message(FATAL_ERROR "capture.json lists ${originCount} origins, expected ${COUNT}")
endif()
if(NOT "${originsA}" STREQUAL "${originsB}")
  message(FATAL_ERROR "the same seeds gave different origins:\n${originsA}\nvs\n${originsB}")
endif()
# capture.json holds 2.5 um as 2499.9999999999995 nm; compare to 1e-6 nm.
math(EXPR halfLowNm "${HALF_SIDE_NM} - 1")
if(NOT (halfSide GREATER "${halfLowNm}.999999" AND halfSide LESS "${HALF_SIDE_NM}.000001"))
  message(FATAL_ERROR "capture.json half side ${halfSide} nm is not ${HALF_SIDE_NM} nm")
endif()
set(halfSide ${HALF_SIDE_NM})

# The face is z = -(h - 1 nm); compare |z| with h - 1 to 1e-6 nm.
math(EXPR faceNm "${halfSide} - 1")
math(EXPR faceBelowNm "${halfSide} - 2")
set(zLow "${faceBelowNm}.999999")
set(zHigh "${faceNm}.000001")
set(xValues "")
foreach(entry IN LISTS originsA)
  string(REGEX MATCH "\\[ *([0-9.eE+-]+), *([0-9.eE+-]+), *([0-9.eE+-]+) *\\]" ignored "${entry}")
  set(x "${CMAKE_MATCH_1}")
  set(y "${CMAKE_MATCH_2}")
  set(z "${CMAKE_MATCH_3}")
  if(x STREQUAL "" OR y STREQUAL "" OR z STREQUAL "")
    message(FATAL_ERROR "cannot parse the origin: ${entry}")
  endif()
  string(REGEX REPLACE "^-" "" zAbs "${z}")
  if(NOT z MATCHES "^-" OR NOT (zAbs GREATER zLow AND zAbs LESS zHigh))
    message(FATAL_ERROR "origin z=${z} nm is not on the face z = -${faceNm} nm: ${entry}")
  endif()
  foreach(coordinate "${x}" "${y}")
    string(REGEX REPLACE "^-" "" absCoordinate "${coordinate}")
    if(NOT absCoordinate LESS halfSide)
      message(FATAL_ERROR "origin coordinate ${coordinate} nm is outside (-${halfSide}, ${halfSide}) nm: ${entry}")
    endif()
  endforeach()
  list(APPEND xValues "${x}")
  message(STATUS "origin ${entry}")
endforeach()

list(REMOVE_DUPLICATES xValues)
list(LENGTH xValues distinctX)
if(distinctX LESS 2)
  message(FATAL_ERROR "all ${COUNT} origins have the same x: the placement is not random")
endif()
message(STATUS "entry face OK: ${COUNT} origins on z = -${faceNm} nm, ${distinctX} distinct x, reproduced by a second capture")
