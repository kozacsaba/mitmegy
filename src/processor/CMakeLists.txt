# Processor

add_executable(processor
  main.cpp

  Engine.cpp
  GuiManager.cpp
  MessageHandler.cpp
  PluginWindow.cpp
)

target_link_libraries(processor
  PRIVATE
    libmitmegy
    vsthost
    protocol
    juce::juce_gui_basics
    juce::juce_audio_processors
)

set_target_properties(processor PROPERTIES
  WIN32_EXECUTABLE "$<CONFIG:Release>"
)

target_link_options(processor PRIVATE
  "$<$<CONFIG:Release>:/ENTRY:wmainCRTStartup>"
)
