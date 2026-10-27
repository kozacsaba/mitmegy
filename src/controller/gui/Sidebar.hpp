#pragma once

#include "juce_gui_basics/juce_gui_basics.h"

namespace mitmegy
{

class RequestManager;

class Sidebar : public juce::Component
{
public:
  Sidebar(RequestManager& request_manager);

  void paint(juce::Graphics& g) override;
  void resized() override;

private:
  RequestManager& request_manager_;

  juce::TextButton btnSavePreset_;
  juce::TextButton btnLoadPreset_;
  juce::TextButton btnSearchPaths_;
  juce::TextButton btnShowLogss_;
  juce::TextButton btnRescanPluginList_;
}; // Sidebar

} // namespace mitmegy
