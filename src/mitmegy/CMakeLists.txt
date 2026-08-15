add_library(libmitmegy STATIC
  Audio.cpp
)

target_compile_features(libmitmegy
  PUBLIC
    cxx_std_20
)

target_include_directories(libmitmegy
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_compile_definitions(libmitmegy
  PRIVATE
    MITMEGY_AUDIO_SINK_NAME="${MITMEGY_AUDIO_SINK_NAME}"
)

target_link_libraries(libmitmegy
  PUBLIC
    ole32
)
