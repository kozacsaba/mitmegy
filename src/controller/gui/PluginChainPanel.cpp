#include "PluginchainPanel.hpp"
#include "protocol/messages.hpp"

#include <cassert>
#include <type_traits>

namespace mitmegy
{

static_assert(std::is_same<PluginID, msg::plugin_runtime_id>::value);

// PluginListModel

PluginListModel::PluginListModel() {}

int PluginListModel::getNumRows()
{
  return plugins_.size();
}

void PluginListModel::paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) {}

juce::Component* PluginListModel::refreshComponentForRow(int rowNumber, bool isRowSelected, juce::Component* existingComponentToUpdate)
{
  if(!juce::isPositiveAndBelow(rowNumber, getNumRows()))
  {
    if(existingComponentToUpdate != nullptr)
      delete existingComponentToUpdate;
    return nullptr;
  }

  juce::ignoreUnused(isRowSelected);

  auto* component = static_cast<PluginSlot*>(existingComponentToUpdate);
  auto& plugin = plugins_[rowNumber];

  if(component == nullptr)
    component = new PluginSlot(*this);

  component->row_ = rowNumber;
  component->runtime_id_ = plugin.runtime_id;
  component->lblPluginName_.setText(plugin.display_name, juce::dontSendNotification);
  component->btnActive_.setToggleState(!plugin.bypassed, juce::dontSendNotification);

  component->btnMoveUp_.setEnabled(rowNumber != 0);
  component->btnMoveDown_.setEnabled(rowNumber != getNumRows()-1);

  return component;
}

void PluginListModel::listBoxItemClicked(int row, const juce::MouseEvent& e)
{
  if(itemSingleClicked_)
    itemSingleClicked_(plugins_[row].runtime_id);
}

void PluginListModel::listBoxItemDoubleClicked(int row, const juce::MouseEvent& e)
{
  if(itemDoubleClicked_)
    itemDoubleClicked_(plugins_[row].runtime_id);
}

// PluginSlot

PluginSlot::PluginSlot(PluginListModel& model)
  : model_(model)
{
  setInterceptsMouseClicks(false, true);

  btnActive_.onClick = [&]()
  {
    if(model_.bypassClicked_)
      model_.bypassClicked_(runtime_id_, !btnActive_.getToggleState());
  };
  btnMoveUp_.onClick = [&]()
  {
    if(model_.moveUpClicked_)
      model_.moveUpClicked_(runtime_id_, row_);
  };
  btnMoveDown_.onClick = [&]()
  {
    if(model_.moveDownClicked_)
      model_.moveDownClicked_(runtime_id_, row_);
  };
  btnRemove_.onClick = [&]()
  {
    if(model_.removeClicked_)
      model_.removeClicked_(runtime_id_);
  };

  lblPluginName_.setEditable(false);
  lblPluginName_.setInterceptsMouseClicks(false, false);
  btnMoveUp_.setButtonText("/\\");
  btnMoveDown_.setButtonText("\\/");
  btnRemove_.setButtonText("X");

  addAndMakeVisible(btnActive_);
  addAndMakeVisible(btnMoveUp_);
  addAndMakeVisible(btnMoveDown_);
  addAndMakeVisible(lblPluginName_);
  addAndMakeVisible(btnRemove_);
}

void PluginSlot::resized()
{
  auto area = getLocalBounds();
  const int buttonWidth = area.getWidth() / 8;

  btnActive_.setBounds(area.removeFromLeft(buttonWidth));
  btnMoveUp_.setBounds(area.removeFromLeft(buttonWidth));
  btnMoveDown_.setBounds(area.removeFromLeft(buttonWidth));

  btnRemove_.setBounds(area.removeFromRight(buttonWidth));

  lblPluginName_.setBounds(area);
}

void PluginSlot::paint(juce::Graphics& g) {}

// PluginChainPanel

PluginChainPanel::PluginChainPanel()
{
  lblActiveChain_.setText("Active Chain", juce::dontSendNotification);
  lbxPluginChain_.setModel(&listModel_);
  btnAddPlugin_.setButtonText("Add Plugin");

  addAndMakeVisible(lblActiveChain_);
  addAndMakeVisible(lbxPluginChain_);
  addAndMakeVisible(btnAddPlugin_);
}

void PluginChainPanel::resized()
{
  auto area = getLocalBounds();
  const int buttonHeight = area.proportionOfHeight(0.2f);

  lblActiveChain_.setBounds(area.removeFromTop(buttonHeight));
  btnAddPlugin_.setBounds(area.removeFromBottom(buttonHeight));
  lbxPluginChain_.setBounds(area);
}

void PluginChainPanel::paint(juce::Graphics& g) {}

} // namespace mitmegy
