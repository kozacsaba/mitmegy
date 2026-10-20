// This class should own a message worker thread, and should expose an async
// portRequest method. GUI should only call this async method, which enques a
// request send and registers the request into an in-flight requests vector.
// requests are either timed out or completed with a reply or an error. The
// RequestManager class handles both, and launches gui updates (async)

#pragma once

#include "MessageHandler.hpp"
#include "Frame.hpp"

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

// TODO: maybe think about only exposing payload, and generate frame headers
// automatically? only Type is needed from header after unpacking, so most
// of the header could be just omitted
class RequestManager
{
public:
  using Deadline = std::chrono::steady_clock::time_point;
  using FrameProcessor = std::function<void(Frame)>; // single use consumer
  struct Task;
  struct Request;

public:
  RequestManager(MessageHandler& messageHandler);
  ~RequestManager();

  void start();
  void stop();
  bool isRunning() const { return running_; }

  // taking Frame by value, because it is consumed. Do not call if not running
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

struct RequestManager::Task
{
  Frame frame;
  FrameProcessor task;
}; // RequestHandler::Task

struct RequestManager::Request
{
  FrameProcessor response_handler;
  Deadline deadline;
}; // RequestHandler::Request

} // namespace mitmegy
