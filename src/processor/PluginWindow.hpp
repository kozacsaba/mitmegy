#pragma once

#include "vsthost/PluginChain.hpp"

#include "juce_gui_basics/juce_gui_basics.h"
#include "juce_audio_processors/juce_audio_processors.h"

#include <functional>

namespace mitmegy
{

class PluginWindow : public juce::DocumentWindow
{
public:
  explicit PluginWindow(PluginChain::Plugin* plugin, std::function<void()> onClose);

  void closeButtonPressed() override;

private:
  PluginChain::Plugin* plugin_;
  std::function<void()> onClose_;
};

} // namespace mitmegy
