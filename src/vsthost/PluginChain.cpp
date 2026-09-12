#include "PluginChain.hpp"

#include <stdexcept>

namespace
{

juce::String getIdentifierString(const juce::AudioPluginInstance* instance)
{
  if(instance == nullptr) 
    throw std::runtime_error("Cannot create identifier string from nullptr AudioPluginInstance");
  const auto description = instance->getPluginDescription();
  return description.createIdentifierString();
}

} // namespace

namespace mitmegy
{

PluginChain::Plugin::Plugin(
  std::unique_ptr<juce::AudioPluginInstance>&& instance,
  double sampleRate, int maxBlockSize
)
  : instance_(std::move(instance))
  , runtime_id_(global_id_++)
  , IdentifierString_(getIdentifierString(instance.get()))
{
  instance_->prepareToPlay(sampleRate, maxBlockSize);
  bypassed_ = false;
}

PluginChain::Plugin::~Plugin()
{
  bypassed_ = true;
  instance_->releaseResources();
  instance_ = nullptr;
}

PluginChain::PluginChain()
  : live_ (&chain_a_)
  , stage_ (&chain_b_)
{}

PluginChain::~PluginChain() = default;

void PluginChain::configure(double sampleRate, int maxBlockSize)
{
  sampleRate_ = sampleRate;
  maxBlockSize_ = maxBlockSize;

  for (const auto& plugin : *live_)
  {
    plugin->instance_->releaseResources();
    plugin->instance_->prepareToPlay(sampleRate_, maxBlockSize_);
  }
}

void PluginChain::processBlock(juce::AudioBuffer<float>& buffer)
{
  if(maxBlockSize_ == 0)
    throw std::runtime_error("PluginChain: Cannot call process: not prepared");

  if(switch_request_)
  {
    stage_ = live_.exchange(stage_);
    switch_request_ = false;
    switch_request_.notify_all();
  }

  for(auto& plugin : *live_)
  {
    if(!plugin->bypassed_)
      plugin->instance_->processBlock(buffer, midi_);
  }
}

void PluginChain::add(int index, std::unique_ptr<juce::AudioPluginInstance>&& instance)
{
  if(index < -1)
    throw std::invalid_argument("Invalid index: " + std::to_string(index) 
      + ". Only valid negative index is -1 (append to end)");

  const size_t size = live_.load()->size();
  if(index > size)
    throw std::invalid_argument("Cannot insert at invalid index " 
      + std::to_string(index) + ", size is only " + std::to_string(size));

  auto plugin = std::make_shared<Plugin>(std::move(instance), sampleRate_, maxBlockSize_);

  // copy live chain
  { // scope
    Chain* stage = stage_.load();
    const Chain* live = live_.load();

    stage->clear();
    stage->reserve(live->size());
    for(const auto& elem : *live)
      stage->push_back(elem);
  } // scope

  if(index == -1)
  {
    stage_.load()->push_back(plugin);
  }
  else
  {
    auto it = stage_.load()->begin();
    stage_.load()->insert(it+index, plugin);
  }

  switch_request_ = true;
  switch_request_.wait(true);
  // wait for processBlock to switch

  try
  {
    stage_.load()->clear();
  }
  catch(const std::exception& e)
  {
    throw std::runtime_error("Error while destroying a plugin instance: "
                             + std::string(e.what()));
  }
}

void PluginChain::remove(PluginID id)
{
  Chain* stage = stage_.load();
  const Chain* live = live_.load();

  stage->clear();
  stage->reserve(live->size());
  for(const auto& elem : *live)
    stage->push_back(elem);

  const auto it = std::find_if(stage->begin(), stage->end(), 
    [&](const auto& elem) -> bool {
      return elem->runtime_id_ == id;
    });

  if(it == stage->end())
    throw std::runtime_error("Cannot remove plugin instance. ID not found.");

  stage->erase(it);

  switch_request_ = true;
  switch_request_.wait(true);
  // wait for processBlock to switch

  try
  {
    stage_.load()->clear();
  }
  catch(const std::exception& e)
  {
    throw std::runtime_error("Error while destroying a plugin instance: "
                             + std::string(e.what()));
  }
}

std::size_t PluginChain::size() const noexcept
{
  // live_ being a nullptr *COULD* throw... leaving that in, because live_ should never
  // be nullptr under healthy operation
  return live_.load()->size();
}

std::shared_ptr<PluginChain::Plugin> PluginChain::plugin_at(size_t index)
{
  return (*live_.load())[index];
}

std::shared_ptr<const PluginChain::Plugin> PluginChain::plugin_at(size_t index) const
{
  return (*live_.load())[index];
}

std::shared_ptr<PluginChain::Plugin> PluginChain::plugin_id(const PluginID& id)
{
  return std::const_pointer_cast<Plugin>(
    std::as_const(*this).plugin_id(id));
}

std::shared_ptr<const PluginChain::Plugin> PluginChain::plugin_id(const PluginID& id) const
{
  const auto& live = *live_.load();
  const auto it = std::find_if(
    live.begin(), live.end(), 
    [&](const std::shared_ptr<PluginChain::Plugin>& elem)
    { return elem->runtime_id_ == id; });

  if(it == live.end())
    return nullptr;
  else
    return *it;
}

} // namespace mitmegy
