#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <cstddef>
#include <memory>
#include <vector>

namespace mitmegy
{

class PluginChain
{
private:
  struct Plugin;

public:
  using PluginID = size_t;

  PluginChain();
  ~PluginChain();

  PluginChain(PluginChain&&) = delete;
  PluginChain(const PluginChain&) = delete;
  PluginChain& operator=(PluginChain&&) = delete;
  PluginChain& operator=(const PluginChain&&) = delete;

  // DO NOT CALL LIVE. Stop the processing, configure, and start it again. 
  void configure(double sampleRate, int maxBlockSize);
  void processBlock(juce::AudioBuffer<float>& buffer);

  // processBlock does the actualy switch
  void add(int index, std::unique_ptr<juce::AudioPluginInstance>&& plugin);
  // processBlock does the actualy switch
  void remove(PluginID id);

  std::size_t size() const noexcept;
  std::shared_ptr<Plugin> plugin_at(size_t index);
  std::shared_ptr<const Plugin> plugin_at(size_t index) const;
  std::shared_ptr<Plugin> plugin_id(const PluginID& id);
  std::shared_ptr<const Plugin> plugin_id(const PluginID& id) const;

private:
  juce::MidiBuffer midi_{};
  double sampleRate_ = 0.0;
  int maxBlockSize_ = 0;

  std::atomic<bool> switch_request_ = false;
  using Chain = std::vector<std::shared_ptr<Plugin>>;

  Chain chain_a_;
  Chain chain_b_;

  std::atomic<Chain*> live_;
  std::atomic<Chain*> stage_;

}; // PluginChain

class PluginChain::Plugin
{
public:
  Plugin (std::unique_ptr<juce::AudioPluginInstance>&& instance,
          double sampleRate, int maxBlockSize);
  ~Plugin();

  std::unique_ptr<juce::AudioPluginInstance> instance_;
  const PluginID runtime_id_;
  const juce::String IdentifierString_; // used for creating the same plugin in another sesison
  std::atomic<bool> bypassed_ = true;

private:
  inline static PluginID global_id_ = 0;
}; // Plugin

} // namespace mitmegy
