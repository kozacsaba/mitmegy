#pragma once

#include "juce_gui_basics/juce_gui_basics.h"

namespace mitmegy
{

class Sidebar : public juce::Component
{
public:
  Sidebar();

  void paint(juce::Graphics& g) override;
  void resized() override;

  juce::TextButton btnSavePreset_;
  juce::TextButton btnLoadPreset_;
  juce::TextButton btnSearchPaths_;
  juce::TextButton btnShowLogs_;
  juce::TextButton btnRescanPluginList_;
}; // Sidebar

} // namespace mitmegy
