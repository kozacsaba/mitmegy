#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace mitmegy
{

class MainComponent : public juce::Component
{
public:
  MainComponent();

  void paint(juce::Graphics& g) override;
};

} // namespace mitmegy
