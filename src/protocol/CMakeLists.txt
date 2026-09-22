# protocol

add_library(protocol STATIC
  Frame.cpp
  messages.cpp
  NamedPipe.cpp
)

target_include_directories(protocol
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)
