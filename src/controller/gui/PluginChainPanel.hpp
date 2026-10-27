#pragma once

#include "juce_gui_basics/juce_gui_basics.h"

namespace mitmegy
{

class RequestManager;

class PluginChainPanel : public juce::Component
{
public:
  PluginChainPanel(RequestManager& request_manager);

  void resized() override;
  void paint(juce::Graphics& g) override;

private:

}; // PluginChainPanel

} // namespace mitmegy
