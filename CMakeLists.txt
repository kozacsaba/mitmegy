# MITMEGY

cmake_minimum_required(VERSION 3.20)
project(wasapi_test LANGUAGES C CXX)
enable_testing()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(/FS)

set(MITMEGY_AUDIO_SINK_NAME "AudioSink" CACHE STRING
  "Substring identifying the virtual audio sink endpoint")

message(STATUS "Audio sink identifier: ${MITMEGY_AUDIO_SINK_NAME}")

add_subdirectory(ext/juce)
add_subdirectory(src/mitmegy)
add_subdirectory(src/vsthost)
add_subdirectory(src/protocol)
add_subdirectory(src/processor)
add_subdirectory(src/controller)
add_subdirectory(tools)
add_subdirectory(tests)

find_program(ISCC
  NAMES ISCC.exe
  PATHS
    "C:/Program Files (x86)/Inno Setup 7"
    "C:/Program Files/Inno Setup 7"
    "C:/Program Files (x86)/Inno Setup 6"
    "C:/Program Files/Inno Setup 6"
)

if(ISCC)
  add_custom_target(installer
    COMMAND "${ISCC}" "${CMAKE_SOURCE_DIR}/installer/setup.iss"
    DEPENDS controller processor
    VERBATIM
  )
else()
  message(STATUS "Inno Setup not found; skipping installer target")
endif()
