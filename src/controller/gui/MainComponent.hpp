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
  struct ListChain;
} // namespace msg

class RequestManager;

class MainComponent : public juce::Component
{
public:
  MainComponent(RequestManager&);

  void paint(juce::Graphics& g) override;
  void resized() override;

private:
  void initContent();

  RequestManager& request_manager_;

  AudioDevicePanel audioDevicePanel_;
  PluginChainPanel pluginChainPanel_;
  Sidebar sidebar_;

  // message thread exclusive callbacks

  void updateAudioDeviceList(const msg::ListDevices& list);
  void audioDeviceSelected();

  void updateActiveChain(const msg::ListChain& list);
  void bypassPlugin(PluginID runtime_id, bool bypass);
  void movePlugin(PluginID runtime_id, int old_index, bool up_one);
  void removePlugin(PluginID runtime_id);
};

} // namespace mitmegy
