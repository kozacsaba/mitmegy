#include "gui/Sidebar.hpp"
#include "gui/tweaks.hpp"
#include "RequestManager.hpp"

namespace mitmegy
{

Sidebar::Sidebar()
{
  btnSavePreset_.setButtonText("Save Preset");
  btnLoadPreset_.setButtonText("Load Preset");
  btnSearchPaths_.setButtonText("VST Paths");
  btnShowLogs_.setButtonText("Logs");
  btnRescanPluginList_.setButtonText("Rescan Plugins");

  btnRescanPluginList_.setClickingTogglesState(false);

  addAndMakeVisible(btnSavePreset_);
  addAndMakeVisible(btnLoadPreset_);
  addAndMakeVisible(btnSearchPaths_);
  addAndMakeVisible(btnShowLogs_);
  addAndMakeVisible(btnRescanPluginList_);
}

void Sidebar::paint(juce::Graphics& g) {}

void Sidebar::resized()
{
  auto area = getLocalBounds();
  const int section_height = area.getHeight() / 5;

  btnSavePreset_.setBounds(area.removeFromTop(section_height).reduced(COMPONENT_MARGIN));
  btnLoadPreset_.setBounds(area.removeFromTop(section_height).reduced(COMPONENT_MARGIN));
  btnSearchPaths_.setBounds(area.removeFromTop(section_height).reduced(COMPONENT_MARGIN));
  btnShowLogs_.setBounds(area.removeFromTop(section_height).reduced(COMPONENT_MARGIN));
  btnRescanPluginList_.setBounds(area.reduced(COMPONENT_MARGIN));
}

} // namespace mitmegy
