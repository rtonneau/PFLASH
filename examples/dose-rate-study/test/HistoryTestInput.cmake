# Writes the inputs of one `history` test (included by RunHistorySmoke.cmake and
# RunHistoryExpectFailure.cmake). Everything lives in WORK_DIR, removed first.
#   WORK_DIR        test directory; the history writes to ${WORK_DIR}/out
#   LIBRARY         packet library made by a capture fixture
#   CONFIG          config lines separated by '|'; @LIBRARY@ and @SCHEDULE@ are
#                   replaced by the library and schedule paths
#   SCHEDULE        optional: schedule rows separated by '|' (written to schedule.txt)
#   PACKET_FIND     optional: use a copy of LIBRARY in which every PACKET_FIND
#   PACKET_REPLACE  of packet-00000.txt is replaced by PACKET_REPLACE
# Sets historyConfig (the config file) and historyOutput (the output directory).
foreach(var HISTORY WORK_DIR LIBRARY CONFIG)
  if(NOT DEFINED ${var})
    message(FATAL_ERROR "HistoryTestInput.cmake: ${var} is not set")
  endif()
endforeach()

file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}")

set(historyLibrary "${LIBRARY}")
if(DEFINED PACKET_FIND)
  # An edited copy; the fixture library itself is never changed.
  set(historyLibrary "${WORK_DIR}/library")
  file(GLOB libraryFiles "${LIBRARY}/*")
  if(NOT libraryFiles)
    message(FATAL_ERROR "the library ${LIBRARY} is empty or missing")
  endif()
  file(COPY ${libraryFiles} DESTINATION "${historyLibrary}")
  file(READ "${historyLibrary}/packet-00000.txt" packetText)
  string(FIND "${packetText}" "${PACKET_FIND}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "'${PACKET_FIND}' is not in packet-00000.txt; the edit would test nothing")
  endif()
  string(REPLACE "${PACKET_FIND}" "${PACKET_REPLACE}" packetText "${packetText}")
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
