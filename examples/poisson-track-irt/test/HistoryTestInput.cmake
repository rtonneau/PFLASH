# Writes the inputs of one `history` test (included by RunHistorySmoke.cmake and
# RunHistoryExpectFailure.cmake). Everything lives in WORK_DIR, removed first.
#   WORK_DIR        test directory; the history writes to ${WORK_DIR}/out
#   LIBRARY         packet library made by a capture fixture
#   CONFIG          config lines separated by '|'; @LIBRARY@ and @SCHEDULE@ are
#                   replaced by the library and schedule paths
#   SCHEDULE        optional: schedule rows separated by '|' (written to schedule.txt)
#   PACKET_FIND     optional: use a copy of LIBRARY in which every PACKET_FIND
#   PACKET_REPLACE  of packet-00000.txt is replaced by PACKET_REPLACE
#   PACKET_DUPLICATE optional: in the copy of LIBRARY, packet 1 is made an exact copy of packet 0
#                   (packet-00001.txt and its library.csv row), so that a schedule using both
#                   at one offset puts every molecule onto its copy
#   PACKET_COINCIDE optional: in the copy of packet-00000.txt, molecule 2 is moved to the exact
#                   position of molecule 1 (two molecules at one point); with
#                   PACKET_COINCIDE_GROUP=<k> both also get productGroup k
# Sets historyConfig (the config file) and historyOutput (the output directory).
foreach(var HISTORY WORK_DIR LIBRARY CONFIG)
  if(NOT DEFINED ${var})
    message(FATAL_ERROR "HistoryTestInput.cmake: ${var} is not set")
  endif()
endforeach()

file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}")

set(historyLibrary "${LIBRARY}")
if(DEFINED PACKET_FIND OR DEFINED PACKET_COINCIDE OR PACKET_DUPLICATE)
  # An edited copy; the fixture library itself is never changed.
  set(historyLibrary "${WORK_DIR}/library")
  file(GLOB libraryFiles "${LIBRARY}/*")
  if(NOT libraryFiles)
    message(FATAL_ERROR "the library ${LIBRARY} is empty or missing")
  endif()
  file(COPY ${libraryFiles} DESTINATION "${historyLibrary}")
  if(PACKET_DUPLICATE)
    file(COPY_FILE "${historyLibrary}/packet-00000.txt" "${historyLibrary}/packet-00001.txt")
    file(STRINGS "${historyLibrary}/library.csv" indexLines)
    list(GET indexLines 0 indexHeader)
    list(GET indexLines 1 indexRow0)
    # <packetIndex>,<file>,<rest of the row of packet 0>
    string(REGEX REPLACE "^0,packet-00000.txt," "1,packet-00001.txt," indexRow1 "${indexRow0}")
    if(indexRow1 STREQUAL indexRow0)
      message(FATAL_ERROR "unexpected library.csv row: ${indexRow0}")
    endif()
    file(WRITE "${historyLibrary}/library.csv" "${indexHeader}
${indexRow0}
${indexRow1}
")
  endif()
  file(READ "${historyLibrary}/packet-00000.txt" packetText)
  if(DEFINED PACKET_FIND)
    string(FIND "${packetText}" "${PACKET_FIND}" found)
    if(found EQUAL -1)
      message(FATAL_ERROR "'${PACKET_FIND}' is not in packet-00000.txt; the edit would test nothing")
    endif()
    string(REPLACE "${PACKET_FIND}" "${PACKET_REPLACE}" packetText "${packetText}")
  endif()
  if(PACKET_COINCIDE)
    # molecule <name> <charge> <status> <productGroup> <nOcc> <occ...> <x y z> ...
    string(REGEX MATCHALL "molecule [^
]*" moleculeLines "${packetText}")
    list(LENGTH moleculeLines moleculeLineCount)
    if(moleculeLineCount LESS 2)
      message(FATAL_ERROR "packet-00000.txt has fewer than two molecules")
    endif()
    list(GET moleculeLines 0 firstLine)
    list(GET moleculeLines 1 secondLine)
    string(REPLACE " " ";" firstTokens "${firstLine}")
    string(REPLACE " " ";" secondTokens "${secondLine}")
    list(GET firstTokens 5 firstOccupancyCount)
    list(GET secondTokens 5 secondOccupancyCount)
    math(EXPR firstX "6 + ${firstOccupancyCount}")
    math(EXPR secondX "6 + ${secondOccupancyCount}")
    foreach(axis RANGE 0 2)
      math(EXPR from "${firstX} + ${axis}")
      math(EXPR to "${secondX} + ${axis}")
      list(GET firstTokens ${from} coordinate)
      list(REMOVE_AT secondTokens ${to})
      list(INSERT secondTokens ${to} "${coordinate}")
    endforeach()
    if(DEFINED PACKET_COINCIDE_GROUP)
      list(REMOVE_AT firstTokens 4)
      list(INSERT firstTokens 4 "${PACKET_COINCIDE_GROUP}")
      list(REMOVE_AT secondTokens 4)
      list(INSERT secondTokens 4 "${PACKET_COINCIDE_GROUP}")
      string(REPLACE ";" " " newFirstLine "${firstTokens}")
      string(REPLACE "${firstLine}" "${newFirstLine}" packetText "${packetText}")
    endif()
    string(REPLACE ";" " " newSecondLine "${secondTokens}")
    string(REPLACE "${secondLine}" "${newSecondLine}" packetText "${packetText}")
  endif()
  file(WRITE "${historyLibrary}/packet-00000.txt" "${packetText}")
endif()

set(historySchedule "${WORK_DIR}/schedule.txt")
if(DEFINED SCHEDULE)
  string(REPLACE "|" "\n" scheduleText "# time_ns packet_index dx_um dy_um dz_um|${SCHEDULE}|")
  file(WRITE "${historySchedule}" "${scheduleText}")
endif()

string(REPLACE "@LIBRARY@" "${historyLibrary}" configText "${CONFIG}")
string(REPLACE "@SCHEDULE@" "${historySchedule}" configText "${configText}")
string(REPLACE "|" "\n" configText "${configText}|")
set(historyConfig "${WORK_DIR}/history.cfg")
file(WRITE "${historyConfig}" "${configText}")
set(historyOutput "${WORK_DIR}/out")
