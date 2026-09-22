#pragma once

#include "protocol/NamedPipe.hpp"
#include "protocol/Frame.hpp"

#include <optional>
#include <thread>
#include <atomic>

namespace mitmegy
{

class Engine;
class PluginChain;

class MessageHandler
{
public:
  MessageHandler(Engine& engine_, PluginChain& chain_);
  ~MessageHandler();

  void start();
  void stop();

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

  Engine& engine_;
  PluginChain& chain_;

  NamedPipeServer server_;
  std::optional<NamedPipeConnection> connection_;
  Framer framer_;

  void worker();
  std::thread thread_;
  std::atomic<bool> running_;

}; // MessageHandler

} // namespace mitmegy
