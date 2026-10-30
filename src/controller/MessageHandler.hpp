#pragma once

#include "protocol/Frame.hpp"
#include "protocol/NamedPipe.hpp"

#include <thread>
#include <atomic>
#include <vector>
#include <string>
#include <mutex>
#include <functional>

namespace mitmegy
{

class MessageHandler
{
public:
  using FrameHandler = std::function<void(Frame)>;

  MessageHandler();
  ~MessageHandler();

  void start();
  void stop();

  bool sendFrame(const Frame& frame);
  void setMessageHandler(FrameHandler handler);

private:
  void processFrame(const Frame& frame);
  void log(const std::string& msg);

  std::mutex frame_handler_mut_;
  FrameHandler frame_handler_;

  NamedPipeClient client_;
  std::mutex connection_mut_;
  std::optional<NamedPipeConnection> connection_;
  Framer framer_;

  void worker();
  std::thread thread_;
  std::atomic<bool> running_;

}; // MessageHandler

} // mitmegy
