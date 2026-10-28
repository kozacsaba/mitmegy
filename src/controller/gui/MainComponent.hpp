#pragma once

#include "AudioDevicePanel.hpp"
#include "PluginChainPanel.hpp"
#include "Sidebar.hpp"

#include <juce_gui_basics/juce_gui_basics.h>

namespace mitmegy
{

namespace msg
{
  struct ListDevices;
} // namespace msg

class RequestManager;

struct AudioDevice
{
  int list_id;
  std::string device_id;
  std::string name;
};

class MainComponent : public juce::Component
{
public:
  MainComponent(RequestManager&);

  void paint(juce::Graphics& g) override;
  void resized() override;

private:
  void init();

  RequestManager& request_manager_;

  AudioDevicePanel audioDevicePanel_;
  PluginChainPanel pluginChainPanel_;
  Sidebar sidebar_;

  // message thread exclusive callbacks
  void updateAudioDeviceList(const msg::ListDevices& list);
  void audioDeviceSelected();

  // message thread exclusive members
  std::vector<AudioDevice> devices_;
};

} // namespace mitmegy
