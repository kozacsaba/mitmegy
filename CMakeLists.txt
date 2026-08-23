# MITMEGY

cmake_minimum_required(VERSION 3.20)
project(wasapi_test LANGUAGES C CXX)

set(MITMEGY_AUDIO_SINK_NAME "AudioSink" CACHE STRING
  "Substring identifying the virtual audio sink endpoint")

message(STATUS "Audio sink identifier: ${MITMEGY_AUDIO_SINK_NAME}")

add_subdirectory(ext/juce)
add_subdirectory(src/mitmegy)
add_subdirectory(src/app)
add_subdirectory(tools)
add_subdirectory(tests)
