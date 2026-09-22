#include "MessageHandler.hpp"
#include "Engine.hpp"

#include "vsthost/PluginChain.hpp"
#include "protocol/messages.hpp"

namespace mitmegy
{

MessageHandler::MessageHandler(Engine& engine, PluginChain& chain)
  : engine_(engine)
  , chain_(chain)
  , server_(PIPENAME)
  , running_(false)
{

}

MessageHandler::~MessageHandler()
{
  stop();
}

void MessageHandler::start()
{
  
}

void MessageHandler::stop()
{

}

void MessageHandler::worker()
{
  std::vector<uint8_t> buffer;
  buffer.resize(4096);
  std::optional<size_t> bytes_received = 0;

  while(running_)
  {
    connection_ = server_.accept();
    if(!connection_)
      continue;

    while((bytes_received = connection_->readSome(buffer)).has_value())
    {
      const size_t received = bytes_received.value();
      if(received == 0)
        continue;

      framer_.pushBytes({buffer.data(), received});

      std::optional<Frame> frame;
      while((frame = framer_.getFrame()).has_value())
        processFrame(frame.value());
    }

    connection_ = std::nullopt;
  }
}

void MessageHandler::processFrame(const Frame& frame)
{
  try
  {
    if(frame.header.version != VERSION)
      throw std::runtime_error(
        "Version mismatch: " + std::to_string(frame.header.version)
        + " vs " + std::to_string(VERSION));

    const auto type = (msg::Type)(frame.header.msg_type);
    const size_t id = frame.header.id;

    switch(type)
    {
      case msg::Type::noop:
        handle_noop(id, frame.payload); break;
      case msg::Type::req_query:
        handle_query(id, frame.payload); break;
      case msg::Type::req_add_plugin:
        handle_add_plugin(id, frame.payload); break;
      case msg::Type::req_bypass_plugin:
        handle_bypass_plugin(id, frame.payload); break;
      case msg::Type::req_remove_plugin:
        handle_remove_plugin(id, frame.payload); break;
      case msg::Type::req_move_plugin:
        handle_move_plugin(id, frame.payload); break;
      case msg::Type::req_open_gui:
        handle_open_gui(id, frame.payload); break;
      case msg::Type::req_rescan:
        handle_rescan(id, frame.payload); break;
      case msg::Type::req_add_path:
        handle_add_path(id, frame.payload); break;
      case msg::Type::req_remove_path:
        handle_remove_path(id, frame.payload); break;
      case msg::Type::req_select_device:
        handle_select_device(id, frame.payload); break;
      case msg::Type::req_save_snapshot:
        handle_save_snapshot(id, frame.payload); break;
      case msg::Type::req_load_snapshot:
        handle_load_snapshot(id, frame.payload); break;
      default:
        sendError(id, "Unrecognized message type: " 
                      + std::to_string((size_t)type));
    }
  }
  catch(const std::exception& e)
  {
    log("Error in processing a frame: " + std::string(e.what()));
  }
}

void MessageHandler::log(const std::string& msg) const
{
  std::cout << msg << std::endl;
}

void MessageHandler::handle_noop          (size_t id, const std::vector<uint8_t>& pl)
{

}

void MessageHandler::handle_query         (size_t id, const std::vector<uint8_t>& pl)
{

}

void MessageHandler::handle_add_plugin    (size_t id, const std::vector<uint8_t>& pl)
{

}

void MessageHandler::handle_bypass_plugin (size_t id, const std::vector<uint8_t>& pl)
{

}

void MessageHandler::handle_remove_plugin (size_t id, const std::vector<uint8_t>& pl)
{

}

void MessageHandler::handle_move_plugin   (size_t id, const std::vector<uint8_t>& pl)
{

}

void MessageHandler::handle_open_gui      (size_t id, const std::vector<uint8_t>& pl)
{

}

void MessageHandler::handle_rescan        (size_t id, const std::vector<uint8_t>& pl)
{

}

void MessageHandler::handle_add_path      (size_t id, const std::vector<uint8_t>& pl)
{

}

void MessageHandler::handle_remove_path   (size_t id, const std::vector<uint8_t>& pl)
{

}

void MessageHandler::handle_select_device (size_t id, const std::vector<uint8_t>& pl)
{

}

void MessageHandler::handle_save_snapshot (size_t id, const std::vector<uint8_t>& pl)
{

}

void MessageHandler::handle_load_snapshot (size_t id, const std::vector<uint8_t>& pl)
{

}

void MessageHandler::sendError(size_t id, const std::string& msg)
{

}

} // namespace mitmegy
