# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "")
  file(REMOVE_RECURSE
  "CMakeFiles\\HardwareVisualizer_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\HardwareVisualizer_autogen.dir\\ParseCache.txt"
  "HardwareVisualizer_autogen"
  )
endif()
