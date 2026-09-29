# Processor

add_executable(processor
  main.cpp

  Engine.cpp
  MessageHandler.cpp
)

target_link_libraries(processor
  PRIVATE
    libmitmegy
    vsthost
    protocol
)
