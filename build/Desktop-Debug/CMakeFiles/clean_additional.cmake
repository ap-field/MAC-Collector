# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Debug")
  file(REMOVE_RECURSE
  "CMakeFiles/mac-collector_autogen.dir/AutogenUsed.txt"
  "CMakeFiles/mac-collector_autogen.dir/ParseCache.txt"
  "mac-collector_autogen"
  )
endif()
