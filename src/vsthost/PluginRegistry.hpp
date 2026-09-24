#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <memory>

namespace mitmegy
{

class PluginRegistry
{
public:
  PluginRegistry();

  // returns failed files
  juce::StringArray scan();

  [[nodiscard]] const juce::KnownPluginList& plugins() const noexcept;

  [[nodiscard]] std::unique_ptr<juce::AudioPluginInstance>
  createInstance(const juce::PluginDescription& description) const;

private:
  juce::AudioPluginFormatManager format_manager_;
  juce::KnownPluginList plugin_list_;

  juce::VST3PluginFormat* vst3_format_ = nullptr;
};

} // namespace mitmegy
