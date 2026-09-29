#pragma once

#include "protocol/Frame.hpp"
#include "protocol/NamedPipe.hpp"

#include <thread>
#include <atomic>
#include <vector>
#include <string>

namespace mitmegy
{

class MessageHandler
{
public:
  MessageHandler();
  ~MessageHandler();

  void start();
  void stop();

  void send_query(size_t subject);
  void send_add_plugin(size_t id, int index);
  void send_bypass(size_t id, bool bypass);
  void send_remove_plugin(size_t id);
  void send_move_plugin(size_t id, int index);
  void send_open_gui(size_t id);
  void send_rescan();
  void send_add_path(const std::string& path);
  void send_remove_path(const std::string& path);
  void send_select_device(const std::string& id);
  void send_save_snapshot(const std::string& path);
  void send_load_snapshot(const std::string& path);

private:
  void processFrame(const Frame& frame);
  void log(const std::string& msg);
  void sendMessage(std::vector<uint8_t>&& payload, size_t type);
  std::atomic<size_t> global_id_ = 0;

  void handle_noop(size_t id, const std::vector<uint8_t>& pl);
  void handle_completed(size_t id, const std::vector<uint8_t>& pl);
  void handle_error(size_t id, const std::vector<uint8_t>& pl);
  void handle_list_plugins(size_t id, const std::vector<uint8_t>& pl);
  void handle_list_chain(size_t id, const std::vector<uint8_t>& pl);
  void handle_list_devices(size_t id, const std::vector<uint8_t>& pl);
  void handle_failed_scans(size_t id, const std::vector<uint8_t>& pl);

  NamedPipeClient client_;
  std::mutex connection_mut_;
  std::optional<NamedPipeConnection> connection_;
  Framer framer_;

  // should probably have an active requests vector here,
  // so that we can track lost messages / timeouts

  void worker();
  std::thread thread_;
  std::atomic<bool> running_;

}; // MessageHandler

} // mitmegy
