// This class should own a message worker thread, and should expose an async
// portRequest method. GUI should only call this async method, which enques a
// request send and registers the request into an in-flight requests vector.
// requests are either timed out or completed with a reply or an error. The
// RequestManager class handles both, and launches gui updates (async)

#pragma once

#include "MessageHandler.hpp"
#include "protocol/Frame.hpp"
#include "protocol/messages.hpp"

#include <thread>
#include <atomic>
#include <mutex>
#include <functional>
#include <deque>
#include <vector>
#include <chrono>
#include <unordered_map>

namespace mitmegy
{

namespace detail
{

using Deadline = std::chrono::steady_clock::time_point;
using FrameProcessor = std::function<void(Frame)>; // single use consumer

struct Task
{
  Frame frame;
  FrameProcessor task;
}; // RequestHandler::Task

struct Request
{
  FrameProcessor response_handler;
  Deadline deadline;
}; // RequestHandler::Request

} // namespace detail

Frame makeFrameFrom(msg::Type type, std::vector<uint8_t> payload);

class RequestManager
{
public:
  using Deadline = detail::Deadline;
  using FrameProcessor = detail::FrameProcessor;
  using Task = detail::Task;
  using Request = detail::Request;

public:
  RequestManager(MessageHandler& messageHandler);
  ~RequestManager();

  void start();
  void stop();
  bool isRunning() const { return running_; }

  // taking Frame by value, because it is consumed. Do not call if not running
  void postRequest(msg::Type type, std::vector<uint8_t> payload, FrameProcessor resp_handler, size_t timeout_ms);

  // Prefer the other overload of the function, since that one handles IDs and versioning automatically
  void postRequest(Frame frame, FrameProcessor resp_handler, size_t timeout_ms);

private:
  MessageHandler& messageHandler_;

  void completeNotSent(Request& req);
  void completeTimeout(Request& req);

  void taskWorker();
  std::thread taskThread_;
  std::atomic<bool> running_;

  // strict. accessed from multiple threads. hold as briefly as possible
  std::mutex que_mut_;
  std::deque<Task> que_;

  // loose. only accessed from worker
  std::mutex req_mut_;
  std::unordered_map<size_t, Request> requests_;

}; // RequestManager

} // namespace mitmegy
