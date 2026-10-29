#pragma once

#include "protocol/messages.hpp"

#include "juce_gui_basics/juce_gui_basics.h"

#include <functional>
#include <optional>

namespace mitmegy
{

class MainComponent;

class PluginSelector : public juce::Component,
                       public juce::ListBoxModel
{
public:
  using ResultHandler = std::function<void(std::optional<size_t>)>;

  PluginSelector(std::vector<msg::plugin_full_desc> plugins, ResultHandler onResult);
  ~PluginSelector() override;

  int getNumRows() override;
  void paintListBoxItem(int row, juce::Graphics& g, int width, int height, bool selected) override;
  void listBoxItemDoubleClicked(int row, const juce::MouseEvent&) override;
  void returnKeyPressed(int row) override;
  void resized() override;

private:
  void accept(int row);
  void finish(std::optional<size_t> result);

  std::vector<msg::plugin_full_desc> entries_;
  ResultHandler onResult_;

  juce::ListBox listBox_;
  juce::TextButton btnCancel_;
  juce::TextButton btnOpen_;
}; // PluginSelector

} // namespace mitmegy
