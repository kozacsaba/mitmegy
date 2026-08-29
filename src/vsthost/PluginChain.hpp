#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <cstddef>
#include <memory>
#include <vector>

namespace mitmegy
{

class PluginChain
{
public:
  using Index = std::size_t;

  void add(std::unique_ptr<juce::AudioPluginInstance> plugin);
  void remove(Index index);

  [[nodiscard]] std::size_t size() const noexcept;
  [[nodiscard]] juce::AudioPluginInstance& plugin(Index index);
  [[nodiscard]] const juce::AudioPluginInstance& plugin(Index index) const;
  void setBypassed(Index index, bool bypassed);
  [[nodiscard]] bool isBypassed(Index index) const;

  void prepareToPlay(double sample_rate, int max_block_size);
  void processBlock(juce::AudioBuffer<float>& buffer);
  void releaseResources();

private:
  struct Plugin
  {
    std::unique_ptr<juce::AudioPluginInstance> instance;
    bool bypassed = false;
  };

  std::vector<Plugin> plugins_;

  bool prepared_ = false;
};

}
