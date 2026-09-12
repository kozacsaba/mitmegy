#include "PluginRegistry.hpp"

#include <stdexcept>

namespace mitmegy
{

PluginRegistry::PluginRegistry()
{
  auto format = std::make_unique<juce::VST3PluginFormat>();

  vst3_format_ = format.get();
  format_manager_.addFormat(std::move(format));
}

juce::StringArray PluginRegistry::scan()
{
  juce::PluginDirectoryScanner scanner(
      plugin_list_,
      *vst3_format_,
      vst3_format_->getDefaultLocationsToSearch(),
      true,
      juce::File{});

  juce::String plugin_name;

  while (scanner.scanNextFile(true, plugin_name))
  {
  }

  return scanner.getFailedFiles();
}

const juce::KnownPluginList&
PluginRegistry::plugins() const noexcept
{
  return plugin_list_;
}

std::unique_ptr<juce::AudioPluginInstance>
PluginRegistry::createInstance(
    const juce::PluginDescription& description) const
{
  // Required by JUCE's generic plugin-format interface.
  // The VST3 implementation currently ignores these values.
  constexpr double initial_sample_rate = 44100.0;
  constexpr int initial_block_size = 512;

  juce::String error;

  auto instance = format_manager_.createPluginInstance(
      description,
      initial_sample_rate,
      initial_block_size,
      error);

  if (!instance)
  {
    throw std::runtime_error(
        "Failed to create plugin '" +
        description.name.toStdString() +
        "': " +
        error.toStdString());
  }

  return instance;
}

}
