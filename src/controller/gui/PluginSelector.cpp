#include "gui/PluginSelector.hpp"
#include "gui/MainComponent.hpp"

namespace mitmegy
{

PluginSelector::PluginSelector(std::vector<msg::plugin_full_desc> plugins, ResultHandler onResult)
  : entries_(std::move(plugins))
  , onResult_(std::move(onResult))
{
  listBox_.setModel(this);
  listBox_.setMultipleSelectionEnabled(false);
  addAndMakeVisible(listBox_);

  btnCancel_.onClick = [this](){ finish(std::nullopt); };
  addAndMakeVisible(btnCancel_);

  btnOpen_.onClick = [this]()
  {
    const int row = listBox_.getSelectedRow();
    if(row == -1)
      accept(0);
    else
      accept(row);
  };
  addAndMakeVisible(btnOpen_);

  setSize(350, 450);
}

PluginSelector::~PluginSelector()
{
  listBox_.setModel(nullptr);
}

int PluginSelector::getNumRows()
{
  return entries_.size();
}

// TODO: use a dedicated component for list elements for a fancier display of
// plugin name + manufacturer + category
void PluginSelector::paintListBoxItem(int row, juce::Graphics& g, int width, int height, bool selected)
{
  if (selected)
    g.fillAll(juce::Colours::darkgrey);

  g.setColour(juce::Colours::white);
  g.drawText(entries_[row].plugin_name, 8, 0, width - 16, height, juce::Justification::centredLeft);
}

void PluginSelector::listBoxItemDoubleClicked(int row, const juce::MouseEvent&)
{
  accept(row);
}

void PluginSelector::returnKeyPressed(int row)
{
  accept(row);
}

void PluginSelector::resized()
{
  auto area = getLocalBounds();
  auto buttonsArea = area.removeFromBottom(35);

  btnCancel_.setBounds(buttonsArea.removeFromLeft(buttonsArea.proportionOfWidth(0.5)));
  btnOpen_.setBounds(buttonsArea);

  listBox_.setBounds(area);
}

void PluginSelector::accept(int row)
{
  if (row >= 0 && row < getNumRows())
    finish(entries_[row].id);
}

void PluginSelector::finish(std::optional<size_t> result)
{
  if (auto* dialog = findParentComponentOfClass<juce::DialogWindow>())
  {
    // Schedule destruction after the current callback.
    juce::Component::SafePointer<juce::DialogWindow> safeDialog(dialog);

    juce::MessageManager::callAsync(
      [safeDialog, handler = std::move(onResult_), result]() mutable
      {
        if (safeDialog == nullptr)
          return;

        safeDialog->exitModalState(0);

        if (handler)
          handler(result);
      });
  }
}

} // namespace mitmegy
