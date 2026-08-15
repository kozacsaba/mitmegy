# APP

add_executable(mitmegy
  main.cpp
)

target_link_libraries(mitmegy
  PRIVATE
    libmitmegy
)
