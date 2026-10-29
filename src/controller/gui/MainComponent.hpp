#pragma once

#include "gui/AudioDevicePanel.hpp"
#include "gui/PluginChainPanel.hpp"
#include "gui/Sidebar.hpp"
#include "protocol/messages.hpp"

#include <juce_gui_basics/juce_gui_basics.h>

#include <vector>

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
  void initContent();

  RequestManager& request_manager_;

  AudioDevicePanel audioDevicePanel_;
  PluginChainPanel pluginChainPanel_;
  Sidebar sidebar_;

  // message thread exclusive callbacks

  void updateAudioDeviceList(const msg::ListDevices& list);
  void audioDeviceSelected();

  void updateActiveChain();
  void bypassPlugin(PluginID runtime_id, bool bypass);
  void movePlugin(PluginID runtime_id, int old_index, bool up_one);
  void removePlugin(PluginID runtime_id);

  void rescanPluginList();

  void openPluginSelector();

  std::vector<msg::plugin_full_desc> plugin_list_;
};

} // namespace mitmegy
