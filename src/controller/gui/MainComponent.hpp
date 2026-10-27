#pragma once

#include "AudioDevicePanel.hpp"
#include "PluginChainPanel.hpp"
#include "Sidebar.hpp"

#include <juce_gui_basics/juce_gui_basics.h>

namespace mitmegy
{

class RequestManager;

class MainComponent : public juce::Component
{
public:
  MainComponent(RequestManager&);

  void paint(juce::Graphics& g) override;
  void resized() override;

private:
  RequestManager& request_manager_;

  AudioDevicePanel audioDevicePanel_;
  PluginChainPanel pluginChainPanel_;
  Sidebar sidebar_;
};

} // namespace mitmegy
