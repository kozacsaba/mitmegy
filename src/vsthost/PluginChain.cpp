#include "PluginChain.hpp"

#include <stdexcept>
#include <chrono>

namespace
{

juce::String getIdentifierString(const juce::AudioPluginInstance* instance)
{
  if(instance == nullptr) 
    throw std::runtime_error("Cannot create identifier string from nullptr AudioPluginInstance");
  const auto description = instance->getPluginDescription();
  return description.createIdentifierString();
}

std::string getDisplayName(const juce::AudioPluginInstance* instance)
{
  if(instance == nullptr)
    throw std::runtime_error("Cannot create display name from nullptr AudioPluginInstance");
  const auto description = instance->getPluginDescription();
  return description.name.toStdString();
}

bool waitForSwitch(std::atomic<mitmegy::PluginChain::SwitchState>& state, size_t timeout_ms = 150)
{
  using State = mitmegy::PluginChain::SwitchState;
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);

  while(state != State::idle)
  {
    auto now = std::chrono::steady_clock::now();

    if(deadline < now)
    {
      auto expected = State::pending;
      const bool aborted = state.compare_exchange_strong(
        expected, State::idle, std::memory_order_acquire);
      
      if(aborted)
        return false;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  return true;
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
  , display_name_(getDisplayName(instance_.get()))
  , IdentifierString_(getIdentifierString(instance_.get()))
{
  instance_->prepareToPlay(sampleRate, maxBlockSize);
  bypassed_ = false;
}

PluginChain::Plugin::~Plugin()
{
  try
  {
    if(beforeDestruction_)
      beforeDestruction_();
  }
  catch (...)
  {
    // Do investigate please. Reaching this is very unhealthy.
    jassertfalse;
  }

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

  auto expected = SwitchState::pending;
  if (switch_request_.compare_exchange_strong(
    expected, SwitchState::in_progress, std::memory_order_acquire))
  {
    stage_ = live_.exchange(stage_);
    switch_request_.store(SwitchState::idle, std::memory_order_release);
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
  if(index > (int)size)
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

  switch_request_ = SwitchState::pending;
  waitForSwitch(switch_request_);

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

  switch_request_ = SwitchState::pending;
  waitForSwitch(switch_request_);

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

void PluginChain::move(PluginID id, size_t index)
{
  if(index < 0)
    throw std::invalid_argument("Invalid index: " + std::to_string(index));

  const size_t size = live_.load()->size();
  if(index >= (int)size)
    throw std::invalid_argument("Cannot insert at invalid index " 
      + std::to_string(index) + ", size is only " + std::to_string(size));

  Chain* stage = stage_.load();
  const Chain* live = live_.load();

  stage->clear();
  stage->reserve(live->size());
  for(const auto& elem : *live)
    stage->push_back(elem);

  const auto it_from = std::find_if(stage->begin(), stage->end(), 
    [&](const auto& elem) -> bool {
      return elem->runtime_id_ == id;
    });

  if(it_from == stage->end())
    throw std::runtime_error("Cannot move plugin instance. ID not found.");

  auto it_to = stage->begin() + index;

  if (it_from < it_to)
    std::rotate(it_from, it_from + 1, it_to + 1);
  else if (it_from > it_to)
    std::rotate(it_to, it_from, it_from + 1);

  switch_request_ = SwitchState::pending;
  waitForSwitch(switch_request_);

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
