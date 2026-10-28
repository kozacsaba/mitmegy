#pragma once

#include "juce_gui_basics/juce_gui_basics.h"

#include <string>
#include <vector>
#include <mutex>

namespace mitmegy
{

// TODO: Keeping this as a separate component class even if it only contains
// one combo box, because we might eventually want to add a refresh button
class AudioDevicePanel : public juce::Component
{
public:
  AudioDevicePanel();

  void resized() override;
  void paint(juce::Graphics& g) override;

  juce::ComboBox cbxDeviceSelector_;
}; // AudioDevicePanel

} // namespace mitmegy
