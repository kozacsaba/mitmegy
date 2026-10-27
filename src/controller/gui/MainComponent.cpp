#include "gui/MainComponent.hpp"
#include "RequestManager.hpp"

namespace mitmegy
{

MainComponent::MainComponent(RequestManager& request_manager)
  : request_manager_(request_manager)
{}

void MainComponent::paint(juce::Graphics& g)
{
  g.fillAll(juce::Colours::darkgrey);

  g.setColour(juce::Colours::white);
  g.setFont(32.0f);

  g.drawText("Hello World", getLocalBounds(), juce::Justification::centred);
}

} // namespace mitmegy
