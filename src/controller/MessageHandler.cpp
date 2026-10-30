#include "MessageHandler.hpp"

#include "protocol/messages.hpp"

#include <stdexcept>
#include <iostream>

namespace mitmegy
{

MessageHandler::MessageHandler()
  : frame_handler_(nullptr)
  , running_(false)
{}

MessageHandler::~MessageHandler()
{
  stop();
}

void MessageHandler::start()
{
  if(running_.exchange(true))
    return;

  thread_ = std::thread(&MessageHandler::worker, this);
}

void MessageHandler::stop()
{
  if(!running_.exchange(false))
    return;

  { // lock connection
    std::scoped_lock lock(connection_mut_);
    if(connection_)
      connection_->cancel();
  } // lock connetionc

  if(thread_.joinable())
    thread_.join();
}

bool MessageHandler::sendFrame(const Frame& frame)
{
  const auto packet = serializeFrame(frame);
  return connection_->writeAll(packet);
}

void MessageHandler::setMessageHandler(FrameHandler handler)
{
  std::scoped_lock lock(frame_handler_mut_);
  frame_handler_ = handler;
}

void MessageHandler::worker()
{
  std::vector<uint8_t> buffer;
  buffer.resize(4096);
  std::optional<size_t> bytes_received = 0;

  while(running_)
  {
    { // lock connection
      std::scoped_lock lock(connection_mut_);
      connection_ = client_.connect(PIPENAME);
      if(!connection_)
        throw std::runtime_error("Server not available for named pipe");
    } // lock connection

    while((bytes_received = connection_->readSome(buffer)).has_value())
    {
      const size_t received = bytes_received.value();
      if(received == 0)
        continue;

      framer_.pushBytes({buffer.data(), received});

      FrameHandler handler_;
      { // lock frame handler
        std::scoped_lock lock(frame_handler_mut_);
        handler_ = frame_handler_;
      } // lock frame handler

      std::optional<Frame> frame;
      while((frame = framer_.getFrame()).has_value())
        if(handler_)
          handler_(std::move(frame.value()));
    }

    { // lock connection
      std::scoped_lock lock(connection_mut_);
      connection_ = std::nullopt;
    } // lock connection
  }
}

void MessageHandler::log(const std::string& msg)
{
  std::cout << msg << std::endl;
}

} // mitmegy
