add_library(libmitmegy STATIC
  AudioEngine.cpp
  AudioFifo.cpp
  Com.cpp
  Wasapi.cpp
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
  PUBLIC
    MITMEGY_AUDIO_SINK_NAME="${MITMEGY_AUDIO_SINK_NAME}"
    NOMINMAX
)

target_link_libraries(libmitmegy
  PUBLIC
    ole32
)
