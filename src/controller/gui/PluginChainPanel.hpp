#pragma once

#include "juce_gui_basics/juce_gui_basics.h"

#include <functional>
#include <vector>

namespace mitmegy
{

using PluginID = size_t;

class PluginListModel : public juce::ListBoxModel
{
public:
  struct Element
  {
    PluginID runtime_id;
    std::string display_name;
    bool bypassed;
  }; // Element

  PluginListModel();

  int getNumRows() override;
  void paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override;
  juce::Component* refreshComponentForRow(int rowNumber, bool isRowSelected, juce::Component* existingComponentToUpdate) override;

  void listBoxItemClicked(int row, const juce::MouseEvent& e) override;
  void listBoxItemDoubleClicked(int row, const juce::MouseEvent& e) override;

  std::function<void(PluginID, bool)> bypassClicked_;
  std::function<void(PluginID, int)> moveUpClicked_;
  std::function<void(PluginID, int)> moveDownClicked_;
  std::function<void(PluginID)> removeClicked_;

  std::function<void()> itemSingleClicked_; // open gui
  std::function<void()> itemDoubleClicked_; // rename slot (?)

  std::vector<Element> plugins_;
}; // PluginListModel

class PluginSlot : public juce::Component
{
public:
  PluginSlot(PluginListModel& model);

  void resized() override;
  void paint(juce::Graphics& g) override;

  // TODO: use svg icons instead of letters
  juce::ToggleButton btnActive_;  // meaning !bypassed
  juce::TextButton btnMoveUp_;    // use an arrow up svg
  juce::TextButton btnMoveDown_;  // use an arrow down svg
  juce::Label lblPluginName_;
  juce::TextButton btnRemove_;    // use a trashcan svg

  PluginID runtime_id_;
  int row_;

private:
  PluginListModel& model_;
}; // PluginSlot

class PluginChainPanel : public juce::Component
{
public:
  PluginChainPanel();

  void resized() override;
  void paint(juce::Graphics& g) override;

  juce::Label lblActiveChain_;
  juce::ListBox lbxPluginChain_;
  PluginListModel listModel_;
}; // PluginChainPanel

} // namespace mitmegy
