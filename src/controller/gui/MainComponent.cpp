#include "gui/MainComponent.hpp"
#include "RequestManager.hpp"
#include "tweaks.hpp"

namespace mitmegy
{

MainComponent::MainComponent(RequestManager& request_manager)
  : request_manager_(request_manager)
  , pluginChainPanel_(request_manager_)
  , sidebar_(request_manager_)
{}

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

} // namespace mitmegy
