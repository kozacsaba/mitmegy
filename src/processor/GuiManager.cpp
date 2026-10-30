#include "GuiManager.hpp"

#include "juce_events/juce_events.h"

namespace mitmegy
{

GuiManager::GuiManager() {}
  
GuiManager::~GuiManager()
{
  stop();
}

void GuiManager::start()
{
  if (message_thread_.joinable())
    return;

  ready_.store(false, std::memory_order_relaxed);

  message_thread_ = std::thread([this]{ message_thread(); });
  ready_.wait(false, std::memory_order_acquire);
}

void GuiManager::stop()
{
  if (!message_thread_.joinable())
    return;

  std::atomic<bool> stopped{false};

  const bool posted = juce::MessageManager::callAsync([this, &stopped]
  {
    windows_.clear();

    stopped.store(true, std::memory_order_release);
    stopped.notify_one();

    juce::MessageManager::getInstance()->stopDispatchLoop();
  });

  if (posted)
    stopped.wait(false, std::memory_order_acquire);

  message_thread_.join();
}

void GuiManager::open(PluginChain::Plugin* plugin)
{
  juce::MessageManager::callAsync([this, plugin]
  {
    auto it = windows_.find(plugin);
    if (it != windows_.end())
    {
      it->second->toFront(true);
      return;
    }

    auto window = std::make_unique<PluginWindow>(plugin, [this, plugin]
    {
      juce::MessageManager::callAsync([this, plugin]
      { windows_.erase(plugin); });
    });

    windows_.emplace(plugin, std::move(window));
  });
}

void GuiManager::close(PluginChain::Plugin* plugin)
{
  if (!message_thread_.joinable())
    return;

  if (juce::MessageManager::getInstance()->isThisTheMessageThread())
  {
    windows_.erase(plugin);
    return;
  }

  std::atomic<bool> closed{false};

  const bool posted = juce::MessageManager::callAsync([this, plugin, &closed]
  {
    windows_.erase(plugin);

    closed.store(true, std::memory_order_release);
    closed.notify_one();
  });

  if (posted)
    closed.wait(false, std::memory_order_acquire);
}

void GuiManager::message_thread()
{
  juce::ScopedJuceInitialiser_GUI juceGui;

  auto* manager = juce::MessageManager::getInstance();
  manager->setCurrentThreadAsMessageThread();

  ready_.store(true, std::memory_order_release);
  ready_.notify_all();

  manager->runDispatchLoop();

  windows_.clear();
}

} // namespace mitmegy
