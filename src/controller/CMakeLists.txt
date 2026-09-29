## Controller

add_executable(controller
  main.cpp

  MessageHandler.cpp
)

target_link_libraries(controller
  PRIVATE
    protocol
)
