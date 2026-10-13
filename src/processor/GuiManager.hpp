#pragma once

#include "PluginWindow.hpp"

#include "vsthost/PluginChain.hpp"

#include <thread>
#include <atomic>
#include <unordered_map>

namespace mitmegy
{

class GuiManager
{
public:
  GuiManager();
  ~GuiManager();

  void start();
  void stop();

  void open(PluginChain::Plugin* plugin);
  void close(PluginChain::Plugin* plugin);

private:
  void message_thread();
  std::thread message_thread_;
  std::atomic<bool> ready_;

  std::unordered_map<PluginChain::Plugin*, std::unique_ptr<PluginWindow>> windows_;

}; // GuiManager

} // namespace mitmegy
