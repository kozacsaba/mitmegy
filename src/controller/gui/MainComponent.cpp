#include "gui/MainComponent.hpp"
#include "gui/tweaks.hpp"
#include "RequestManager.hpp"
#include "protocol/messages.hpp"

#include "juce_events/juce_events.h"

#include <stdexcept>

#define ASSERT_MESSAGE_THREAD                                                   \
  if(!juce::MessageManager::getInstance()->isThisTheMessageThread())            \
    throw std::logic_error(std::string(__func__) +                              \
                           " must be called from the message thread")           \

namespace mitmegy
{

constexpr int NoneSelectedID = 0;

MainComponent::MainComponent(RequestManager& request_manager)
  : request_manager_(request_manager)
  , pluginChainPanel_(request_manager_)
  , sidebar_(request_manager_)
{
  init();

  addAndMakeVisible(audioDevicePanel_);
  addAndMakeVisible(sidebar_);
  addAndMakeVisible(pluginChainPanel_);
}

void MainComponent::paint(juce::Graphics& g)
{
  g.fillAll(juce::Colours::darkgrey);
}

void MainComponent::resized()
{
  auto area = getLocalBounds().reduced(COMPONENT_MARGIN, COMPONENT_MARGIN);

  audioDevicePanel_.setBounds(area.removeFromTop(area.proportionOfHeight(1.f/6.f)));
  sidebar_.setBounds(area.removeFromRight(area.proportionOfWidth(0.25)));
  pluginChainPanel_.setBounds(area);
}

void MainComponent::init()
{
  msg::Query command { msg::query_subject::audio_devices };
  auto payload = msg::to_bytes(command);
  request_manager_.postRequest(
    msg::Type::req_query, payload,
    [&](Frame frame)
    {
      if(frame.header.msg_type != (size_t) msg::Type::res_query_devices)
        return; // maybe log?

      msg::ListDevices list = msg::from_bytes<msg::ListDevices>(frame.payload);
      juce::MessageManager::callAsync([this, list = std::move(list)]
        { updateAudioDeviceList(list); });
    },
    200
  );

  // ...
}

void MainComponent::updateAudioDeviceList(const msg::ListDevices& list)
{
  ASSERT_MESSAGE_THREAD;

  auto& cbx = audioDevicePanel_.cbxDeviceSelector_;
  cbx.clear(juce::dontSendNotification);
  devices_.clear();
  int list_id = 1;

  devices_.reserve(list.devices.size());
  for(const auto& dev : list.devices)
  {
    devices_.push_back({list_id, dev.id, dev.name});
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
    devices_.begin(), devices_.end(), 
    [&](const AudioDevice& elem)
    { return elem.list_id == list_id; });

  if(it == devices_.end() || it->device_id.empty())
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

      juce::MessageManager::callAsync([&]()
      { audioDevicePanel_.cbxDeviceSelector_.setSelectedId(id, juce::dontSendNotification); });
    },
    200);
}

} // namespace mitmegy
