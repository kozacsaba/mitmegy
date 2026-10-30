#include "AudioDevicePanel.hpp"
#include "tweaks.hpp"

namespace mitmegy
{

AudioDevicePanel::AudioDevicePanel()
{
  addAndMakeVisible(cbxDeviceSelector_);
}

void AudioDevicePanel::resized()
{
  cbxDeviceSelector_.setBounds(getLocalBounds().reduced(COMPONENT_MARGIN));
}

void AudioDevicePanel::paint(juce::Graphics& g)
{}

} // namespace mitmegy
