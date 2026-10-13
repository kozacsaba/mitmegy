## Controller

juce_add_gui_app(controller
  PRODUCT_NAME "controller"
  VERSION 0.1.0
)

target_sources(controller PRIVATE
  main.cpp

  ControllerApp.cpp
  MessageHandler.cpp

  gui/MainComponent.cpp
)

target_include_directories(controller PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}
)

target_compile_definitions(controller PRIVATE
    MITMEGY_VERSION="$<TARGET_PROPERTY:controller,JUCE_VERSION>"
)

target_link_libraries(controller PRIVATE
  protocol
  juce::juce_gui_basics
)
