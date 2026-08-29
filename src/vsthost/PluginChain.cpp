#include "PluginChain.hpp"

#include <stdexcept>

namespace mitmegy
{

void PluginChain::add(std::unique_ptr<juce::AudioPluginInstance> plugin)
{
  if (prepared_)
    throw std::logic_error("Cannot add plugin while chain is prepared");

  if (!plugin)
    throw std::invalid_argument("Plugin instance is null");

  plugins_.push_back({std::move(plugin), false});
}

void PluginChain::remove(Index index)
{
  if (prepared_)
    throw std::logic_error("Cannot remove plugin while chain is prepared");

  if (index >= plugins_.size())
    throw std::out_of_range("Plugin index out of range");

  plugins_.erase(plugins_.begin() + index);
}

std::size_t PluginChain::size() const noexcept
{
  return plugins_.size();
}

juce::AudioPluginInstance& PluginChain::plugin(Index index)
{
  if (index >= plugins_.size())
    throw std::out_of_range("Plugin index out of range");

  return *plugins_[index].instance;
}

const juce::AudioPluginInstance& PluginChain::plugin(Index index) const
{
  if (index >= plugins_.size())
    throw std::out_of_range("Plugin index out of range");

  return *plugins_[index].instance;
}

void PluginChain::setBypassed(Index index, bool bypassed)
{
  if (index >= plugins_.size())
    throw std::out_of_range("Plugin index out of range");

  plugins_[index].bypassed = bypassed;
}

bool PluginChain::isBypassed(Index index) const
{
  if (index >= plugins_.size())
    throw std::out_of_range("Plugin index out of range");

  return plugins_[index].bypassed;
}

void PluginChain::prepareToPlay(double sample_rate, int max_block_size)
{
  if (prepared_)
    throw std::logic_error("Plugin chain is already prepared");

  for (auto& plugin : plugins_)
    plugin.instance->prepareToPlay(sample_rate, max_block_size);

  prepared_ = true;
}

void PluginChain::processBlock(juce::AudioBuffer<float>& buffer)
{
  jassert(prepared_);
  juce::MidiBuffer midi{};

  if (!prepared_)
    return;

  for (auto& plugin : plugins_)
    if (!plugin.bypassed)
      plugin.instance->processBlock(buffer, midi);
}

void PluginChain::releaseResources()
{
  if (!prepared_)
    return;

  for (auto& plugin : plugins_)
    plugin.instance->releaseResources();

  prepared_ = false;
}

}
