#include "PluginWindow.hpp"

#include <stdexcept>

namespace mitmegy
{

PluginWindow::PluginWindow(PluginChain::Plugin* plugin, std::function<void()> onClose)
  : DocumentWindow(
    plugin ? plugin->display_name_ : "", 
    juce::Colours::darkgrey, 
    juce::DocumentWindow::closeButton)
  , onClose_(std::move(onClose))
{
  if(plugin == nullptr)
    throw std::runtime_error("Cannot create PluginWindow from nullptr plugin");

  if(plugin->instance_ == nullptr)
    throw std::runtime_error("Cannot create PluginWindow from plugin with nullptr instance");

  auto* editor = plugin->instance_->createEditorIfNeeded();
  if(editor == nullptr)
    throw std::runtime_error("Plugin failed to create editor");

  setUsingNativeTitleBar(true);
  setContentOwned(editor, true);
  setResizable(editor->isResizable(), false);

  centreWithSize(getWidth(), getHeight());
  setVisible(true);
}

void PluginWindow::closeButtonPressed()
{
  if (onClose_)
    onClose_();
}

} // namespace mitmegy
