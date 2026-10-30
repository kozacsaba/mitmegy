#include "gui/MainComponent.hpp"
#include "gui/PluginSelector.hpp"
#include "gui/tweaks.hpp"
#include "RequestManager.hpp"
#include "protocol/messages.hpp"

#include "juce_events/juce_events.h"

#include <stdexcept>
#include <algorithm> 

#define ASSERT_MESSAGE_THREAD                                                   \
  if(!juce::MessageManager::getInstance()->isThisTheMessageThread())            \
    throw std::logic_error(std::string(__func__) +                              \
                           " must be called from the message thread")           \

namespace
{

void save_preset(mitmegy::RequestManager& request_manager)
{
  // TODO
}

void load_preset(mitmegy::RequestManager& request_manager)
{
  // TODO
}

void edit_search_paths(mitmegy::RequestManager& request_manager)
{
  // TODO:
}

void show_logs()
{
  // TODO:
}

void open_gui_for(mitmegy::PluginID runtime_id, mitmegy::RequestManager& request_manager)
{
  mitmegy::msg::OpenGui command { runtime_id };
  auto payload = mitmegy::msg::to_bytes(command);
  request_manager.postRequest(
    mitmegy::msg::Type::req_open_gui, std::move(payload),
    nullptr, mitmegy::TIMEOUT_NORMAL);
}

} // namespace

namespace mitmegy
{

constexpr int NoneSelectedID = 0;

MainComponent::MainComponent(RequestManager& request_manager)
  : request_manager_(request_manager)
{
  audioDevicePanel_.cbxDeviceSelector_.onChange = [&](){ audioDeviceSelected(); };

  pluginChainPanel_.btnAddPlugin_.onClick = [&](){ openPluginSelector(); };

  auto& model = pluginChainPanel_.listModel_;
  model.bypassClicked_ = [&](PluginID runtime_id, bool bypassed)
    { bypassPlugin(runtime_id, bypassed); };
  model.moveUpClicked_ = [&](PluginID runtime_id, int old_index)
    { movePlugin(runtime_id, old_index, true); };
  model.moveDownClicked_ = [&](PluginID runtime_id, int old_index)
    { movePlugin(runtime_id, old_index, false); };
  model.removeClicked_ = [&](PluginID runtime_id)
    { removePlugin(runtime_id); };

  model.itemSingleClicked_ = [&](PluginID runtime_id)
    { open_gui_for(runtime_id, request_manager_); };
  // model.itemDoubleClicked_; // TODO: change display name

  sidebar_.btnSavePreset_.onClick = [&](){ save_preset(request_manager_); };
  sidebar_.btnLoadPreset_.onClick = [&](){ load_preset(request_manager_); };
  sidebar_.btnSearchPaths_.onClick = [&](){ edit_search_paths(request_manager_); };
  sidebar_.btnShowLogs_.onClick = [&](){ show_logs(); };
  sidebar_.btnRescanPluginList_.onClick = [&](){ rescanPluginList(); };

  initContent();

  addAndMakeVisible(audioDevicePanel_);
  addAndMakeVisible(sidebar_);
  addAndMakeVisible(pluginChainPanel_);
}

void MainComponent::paint(juce::Graphics& g)
{
  g.fillAll(BACKGROUND_COLOR);
}

void MainComponent::resized()
{
  auto area = getLocalBounds().reduced(COMPONENT_MARGIN, COMPONENT_MARGIN);

  audioDevicePanel_.setBounds(area.removeFromTop(area.proportionOfHeight(1.f/6.f)));
  sidebar_.setBounds(area.removeFromRight(area.proportionOfWidth(0.25)));
  pluginChainPanel_.setBounds(area);
}

void MainComponent::initContent()
{
  { // scope
    msg::Query command { msg::query_subject::audio_devices };
    auto payload = msg::to_bytes(command);
    request_manager_.postRequest(
      msg::Type::req_query, payload,
      [&](Frame frame)
      {
        if(frame.header.msg_type != (size_t) msg::Type::res_query_devices)
          return; // maybe log?

        auto list = msg::from_bytes<msg::ListDevices>(frame.payload);
        juce::MessageManager::callAsync([this, list = std::move(list)]
          { updateAudioDeviceList(list); });
      },
      TIMEOUT_NORMAL
    );
  } // scope

  updateActiveChain();
  rescanPluginList();
}

void MainComponent::updateAudioDeviceList(const msg::ListDevices& list)
{
  ASSERT_MESSAGE_THREAD;

  auto& cbx = audioDevicePanel_.cbxDeviceSelector_;
  cbx.clear(juce::dontSendNotification);
  audioDevicePanel_.devices_.clear();
  int list_id = 1;

  audioDevicePanel_.devices_.reserve(list.devices.size());
  for(const auto& dev : list.devices)
  {
    audioDevicePanel_.devices_.push_back({list_id, dev.id, dev.name});
    cbx.addItem(dev.name, list_id);
    list_id++;
  }

  // TODO: retain selection (use device id)

  cbx.setSelectedId(NoneSelectedID, juce::dontSendNotification);
}

void MainComponent::audioDeviceSelected()
{
  ASSERT_MESSAGE_THREAD;

  const int list_id = audioDevicePanel_.cbxDeviceSelector_.getSelectedId();
  if(list_id == NoneSelectedID)
    return; // none selected

  const auto it = std::find_if(
    audioDevicePanel_.devices_.begin(), audioDevicePanel_.devices_.end(), 
    [&](const AudioDevice& elem)
    { return elem.list_id == list_id; });

  if(it == audioDevicePanel_.devices_.end() || it->device_id.empty())
    throw std::runtime_error("ComboBox selected a device with no ID");

  msg::SelectDevice command { it->device_id };
  auto payload = to_bytes(command);
  request_manager_.postRequest(
    msg::Type::req_select_device, std::move(payload),
    [&, list_id](Frame frame)
    {
      int id = list_id;

      if(frame.header.msg_type != (size_t) msg::Type::res_completed)
        id = NoneSelectedID; // deselect on failure

      juce::MessageManager::callAsync([&, id]()
      { audioDevicePanel_.cbxDeviceSelector_.setSelectedId(id, juce::dontSendNotification); });
    },
    TIMEOUT_MEDIUM);
}

void MainComponent::updateActiveChain()
{
  msg::Query command { msg::query_subject::active_chain };
  auto payload = msg::to_bytes(command);
  request_manager_.postRequest(
    msg::Type::req_query, std::move(payload),
    [&](Frame frame)
    {
      if(frame.header.msg_type != (size_t) msg::Type::res_query_chain)
      {
        jassertfalse;
        return;
      }

      auto list = msg::from_bytes<msg::ListChain>(frame.payload);
      juce::MessageManager::callAsync(
        [this, list = std::move(list)]
        {
          auto& model = pluginChainPanel_.listModel_;
          model.plugins_.clear();
          model.plugins_.reserve(list.plugins.size());

          for(const auto& elem : list.plugins)
          {
            PluginListModel::Element plugin;
            plugin.bypassed = elem.bypassed;
            plugin.display_name = elem.display_name;
            plugin.runtime_id = elem.id;

            model.plugins_.push_back(std::move(plugin));
          }

          pluginChainPanel_.lbxPluginChain_.updateContent();
          pluginChainPanel_.lbxPluginChain_.repaint();
        });
    },
    TIMEOUT_NORMAL);
}

void MainComponent::bypassPlugin(PluginID runtime_id, bool bypass)
{
  msg::BypassPlugin command { runtime_id, bypass };
  auto payload = msg::to_bytes(command);

  request_manager_.postRequest(
    msg::Type::req_bypass_plugin, std::move(payload),
    [this](Frame)
    {
      // TODO: log here too i guess
      updateActiveChain();
    },
    TIMEOUT_NORMAL);
}

void MainComponent::movePlugin(PluginID runtime_id, int old_index, bool up_one)
{
  const int new_index = up_one 
    ? old_index - 1
    : old_index + 1;

  msg::MovePlugin command { runtime_id, new_index };
  auto payload = msg::to_bytes(command);

  request_manager_.postRequest(
    msg::Type::req_move_plugin, std::move(payload),
    [this](Frame)
    {
      updateActiveChain();
    },
    TIMEOUT_NORMAL
  );
}

void MainComponent::removePlugin(PluginID runtime_id)
{
  msg::RemovePlugin command { runtime_id };
  auto payload = msg::to_bytes(command);

  request_manager_.postRequest(
    msg::Type::req_remove_plugin, std::move(payload),
    [this, runtime_id](Frame frame)
    {
      if(frame.header.msg_type != (size_t) msg::Type::res_completed)
        return;

      juce::MessageManager::callAsync([this, runtime_id]()
      {
        auto& model = pluginChainPanel_.listModel_;
        auto it = std::find_if(
          model.plugins_.begin(), model.plugins_.end(),
          [&](const PluginListModel::Element& elem)
          { return elem.runtime_id == runtime_id; });

        if(it == model.plugins_.end())
          return;

        model.plugins_.erase(it);
        pluginChainPanel_.lbxPluginChain_.updateContent();
        pluginChainPanel_.lbxPluginChain_.repaint();
      });     
    },
    TIMEOUT_NORMAL);
}

void MainComponent::rescanPluginList()
{
  sidebar_.btnRescanPluginList_.setToggleState(true, juce::dontSendNotification);
  sidebar_.btnRescanPluginList_.setEnabled(false);

  msg::Rescan command {};
  auto payload = msg::to_bytes(command);
  request_manager_.postRequest(
    msg::Type::req_rescan, std::move(payload),
    [this](Frame frame)
    {
      if(frame.header.msg_type != (size_t) msg::Type::res_failed_plugins)
      {
        // rescan failed
        juce::MessageManager::callAsync([this]
        {
          sidebar_.btnRescanPluginList_.setToggleState(false, juce::dontSendNotification);
          sidebar_.btnRescanPluginList_.setEnabled(true);
        });
        return;
      }

      // TODO: process failed plugins somehow

      msg::Query command { msg::query_subject::plugin_list };
      auto payload = to_bytes(command);
      request_manager_.postRequest(
        msg::Type::req_query, std::move(payload),
        [this](Frame frame)
        {
          if(frame.header.msg_type != (size_t) msg::Type::res_query_plugins)
          {
            // plugin list query failed
            juce::MessageManager::callAsync([this]
            {
              sidebar_.btnRescanPluginList_.setToggleState(false, juce::dontSendNotification);
              sidebar_.btnRescanPluginList_.setEnabled(true);
            });
            return;
          }

          auto list = msg::from_bytes<msg::ListPlugins>(frame.payload);

          juce::MessageManager::callAsync(
            [this, list = std::move(list)] mutable
            { 
              std::swap(plugin_list_, list.plugins);
              sidebar_.btnRescanPluginList_.setToggleState(false, juce::dontSendNotification);
              sidebar_.btnRescanPluginList_.setEnabled(true);
            });
        },
        TIMEOUT_NORMAL);
    },
    TIMEOUT_LONG);
}

void MainComponent::openPluginSelector()
{
  juce::DialogWindow::LaunchOptions options;

  options.content.setOwned(new PluginSelector(
    plugin_list_,
    [this](std::optional<size_t> plugin_id)
    {
      if(!plugin_id.has_value())
        return;

      msg::AddPlugin command {*plugin_id, -1};
      auto payload = msg::to_bytes(command);
      request_manager_.postRequest(
        msg::Type::req_add_plugin, std::move(payload),
        [this](Frame frame)
        {
          // if(frame.header.msg_type != (size_t) msg::Type::res_completed) log what happened
          updateActiveChain();
        },
        TIMEOUT_MEDIUM);
    }
  ));

  options.dialogTitle = "Select Plugin";
  options.componentToCentreAround = this;
  options.escapeKeyTriggersCloseButton = true;
  options.resizable = true;

  options.launchAsync();
}

} // namespace mitmegy
