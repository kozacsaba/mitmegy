#include "RequestManager.hpp"

#include "protocol/messages.hpp"

#include <optional>

namespace
{

mitmegy::Frame makeErrorFrame(const std::string& message)
{
  mitmegy::msg::Error response;
  response.message = message;
  auto payload = mitmegy::msg::to_bytes(response);

  mitmegy::Frame frame;
  frame.payload = std::move(payload);
  frame.header.length = frame.payload.size();
  frame.header.msg_type = (size_t) mitmegy::msg::Type::res_error;
  frame.header.version = mitmegy::VERSION;

  return frame;
}

} // namespace

namespace mitmegy
{

RequestManager::RequestManager(MessageHandler& messageHandler)
  : messageHandler_(messageHandler)
  , running_(false)
{
  messageHandler_.setMessageHandler([this](Frame frame)
  {
    Task task;
    task.frame = std::move(frame);
    task.task = [this](Frame frame)
    {
      const size_t id = frame.header.id;
      Request req;
      
      { // lock req
        if(!requests_.contains(id))
          return;

        req = std::move(requests_[id]);
        requests_.erase(id);
      } // lock req

      if(req.response_handler)
        req.response_handler(frame);
    };

    { // lock que
      std::scoped_lock lock(que_mut_);
      que_.push_back(std::move(task));
    } // lock que
  });
}

RequestManager::~RequestManager()
{
  stop();
}

void RequestManager::start()
{
  if(running_.exchange(true))
    return;

  taskThread_ = std::thread([&](){ taskWorker(); });
}

void RequestManager::stop()
{
  if(!running_.exchange(false))
    return;

  if(taskThread_.joinable())
    taskThread_.join();

  que_.clear();
  requests_.clear();
}

void RequestManager::postRequest(
  Frame frame, FrameProcessor resp_handler, size_t timeout_ms)
{
  Deadline deadline = std::chrono::steady_clock::now() 
                    + std::chrono::milliseconds(timeout_ms);
  Request req = { std::move(resp_handler), std::move(deadline) };
  const size_t id = frame.header.id; 

  { // lock req
    std::scoped_lock lock(req_mut_);
    requests_[id] = std::move(req);
  } // lock req

  Task task;
  task.frame = std::move(frame);
  task.task = [this](Frame frame)
  {
    const bool sent = messageHandler_.sendFrame(frame);
    if(sent)
      return;

    const size_t id = frame.header.id;
    Request req;

    { // lock req
      std::scoped_lock lock(req_mut_);
      if(!requests_.contains(id))
        return;

      req = requests_[id];
    } // lock req

    completeNotSent(req);
  };

  { // lock que
    std::scoped_lock lock(que_mut_);
    que_.push_back(std::move(task));
  } // lock que
}

void RequestManager::completeNotSent(Request& req)
{
  Frame frame = makeErrorFrame("Not sent");
  if(req.response_handler)
    req.response_handler(std::move(frame));
}

void RequestManager::completeTimeout(Request& req)
{
  Frame frame = makeErrorFrame("Timout");
  if(req.response_handler)
    req.response_handler(std::move(frame));
}

void RequestManager::taskWorker()
{
  const auto expiry_check_period = std::chrono::milliseconds(100);
  Deadline next_expiry_check =
    std::chrono::steady_clock::now();

  while(running_)
  {
    // remove expired requests -----

    auto now = std::chrono::steady_clock::now();
    if(now > next_expiry_check)
    {
      std::vector<Request> expired_requests;

      { // lock req
        std::scoped_lock lock(req_mut_);
        expired_requests.resize(requests_.size());
        for(auto& [id, req] : requests_)
        {
          if(req.deadline < now)
          {
            expired_requests.push_back(std::move(requests_[id]));
            requests_.erase(id);
          }
        }
      } // lock req

      for(auto& req : expired_requests)
        completeTimeout(req);

      next_expiry_check += expiry_check_period;

      // severe lateny recovery
      if(next_expiry_check < now)
        next_expiry_check = now + expiry_check_period;
    }

    // process one task -----

    std::optional<Task> task = std::nullopt;

    { // lock que
      std::scoped_lock lock(que_mut_);
      if(!que_.empty())
      {
        task = std::move(que_.front());
        que_.pop_front();
      }
    } // lock que

    if(!task.has_value())
    {
      std::this_thread::sleep_until(next_expiry_check);
      continue;
    }

    if(task->task)
      task->task(task->frame);
  }
}

} // naemspace mitmegy
