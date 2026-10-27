#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace mitmegy
{

class RequestManager;

class MainComponent : public juce::Component
{
public:
  MainComponent(RequestManager&);

  void paint(juce::Graphics& g) override;

private:
  RequestManager& request_manager_;
};

} // namespace mitmegy
