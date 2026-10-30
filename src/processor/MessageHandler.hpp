#pragma once

#include "GuiManager.hpp"

#include "protocol/NamedPipe.hpp"
#include "protocol/Frame.hpp"
#include "vsthost/PluginRegistry.hpp"

#include <optional>
#include <thread>
#include <atomic>
#include <unordered_map>

namespace mitmegy
{

class Engine;
class PluginChain;

class MessageHandler
{
public:
  MessageHandler(Engine& engine, PluginChain& chain, PluginRegistry& registry, GuiManager& gui_manager);
  ~MessageHandler();

  void start();
  void stop();

  // TODO:
  // init --------------------
  // scanPlugins()
  // io ----------------------
  // saveScannedPlugins()
  // loadScannedPlugins()
  // savePluginPaths()
  // loadPluginPaths()

private:
  void processFrame(const Frame& frame);
  void log(const std::string& msg) const;

  void handle_noop          (size_t id, const std::vector<uint8_t>& pl);
  void handle_query         (size_t id, const std::vector<uint8_t>& pl);
  void handle_add_plugin    (size_t id, const std::vector<uint8_t>& pl);
  void handle_bypass_plugin (size_t id, const std::vector<uint8_t>& pl);
  void handle_remove_plugin (size_t id, const std::vector<uint8_t>& pl);
  void handle_move_plugin   (size_t id, const std::vector<uint8_t>& pl);
  void handle_open_gui      (size_t id, const std::vector<uint8_t>& pl);
  void handle_rescan        (size_t id, const std::vector<uint8_t>& pl);
  void handle_add_path      (size_t id, const std::vector<uint8_t>& pl);
  void handle_remove_path   (size_t id, const std::vector<uint8_t>& pl);
  void handle_select_device (size_t id, const std::vector<uint8_t>& pl);
  void handle_save_snapshot (size_t id, const std::vector<uint8_t>& pl);
  void handle_load_snapshot (size_t id, const std::vector<uint8_t>& pl);

  void sendError(size_t id, const std::string& msg);
  void sendSuccess(size_t id);
  void sendMessage(std::vector<uint8_t>&& payload, size_t type, size_t id);

  Engine& engine_;
  PluginChain& chain_;
  PluginRegistry& registry_;
  GuiManager& gui_manager_;
  
  std::unordered_map<size_t, juce::PluginDescription> scannedPlugins_;
  size_t global_plugin_id_ = 0;

  NamedPipeServer server_;
  std::mutex connection_mut_;
  std::optional<NamedPipeConnection> connection_;
  Framer framer_;

  void worker();
  std::thread thread_;
  std::atomic<bool> running_;

}; // MessageHandler

} // namespace mitmegy
