#pragma once

#include "juce_gui_basics/juce_gui_basics.h"

#include <string>
#include <vector>
#include <mutex>

namespace mitmegy
{

class RequestManager;

struct AudioDevice
{
  int list_id;
  std::string device_id;
};

class AudioDevicePanel : public juce::Component
{
public:
  AudioDevicePanel(RequestManager& request_manager);

  void resized() override;
  void paint(juce::Graphics& g) override;

private:
  RequestManager& request_manager_;

  juce::ComboBox cbxDeviceSelector_;
  std::vector<AudioDevice> devices_;
  std::mutex mut_;

}; // AudioDevicePanel

} // namespace mitmegy
