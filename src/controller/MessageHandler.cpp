#include "MessageHandler.hpp"

#include "protocol/messages.hpp"

#include <stdexcept>
#include <iostream>

namespace mitmegy
{

MessageHandler::MessageHandler()
  : running_(false)
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

void MessageHandler::processFrame(const Frame& frame)
{
  size_t id = (size_t)(-1);

  try
  {
    if(frame.header.version != VERSION)
      throw std::runtime_error(
        "Version mismatch: " + std::to_string(frame.header.version)
        + " vs " + std::to_string(VERSION));

    const auto type = (msg::Type)(frame.header.msg_type);
    id = frame.header.id;

    switch(type)
    {
      case msg::Type::noop:
        handle_noop(id, frame.payload); break;
      case msg::Type::res_completed:
        handle_completed(id, frame.payload); break;
      case msg::Type::res_error:
        handle_error(id, frame.payload); break;
      case msg::Type::res_query_plugins:
        handle_list_plugins(id, frame.payload); break;
      case msg::Type::res_query_chain:
        handle_list_chain(id, frame.payload); break;
      case msg::Type::res_query_devices:
        handle_list_devices(id, frame.payload); break;
      case msg::Type::res_failed_plugins:
        handle_failed_scans(id, frame.payload); break;
      default:
        log("Unrecognized message type: " + std::to_string((size_t)type));
    }
  }
  catch(const std::exception& e)
  {
    log("Error in processing a frame: " + std::string(e.what()));
  }
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

      std::optional<Frame> frame;
      while((frame = framer_.getFrame()).has_value())
        processFrame(frame.value());
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

void MessageHandler::sendMessage(std::vector<uint8_t>&& payload, size_t type)
{
  Frame frame;
  frame.payload = std::move(payload);
  frame.header.id = global_id_++;
  frame.header.length = frame.payload.size();
  frame.header.msg_type = type;
  frame.header.version = VERSION;

  const auto packet = serializeFrame(frame);
  connection_->writeAll(packet);
}

// MARK: sends

void MessageHandler::send_query(size_t subject)
{
  using namespace msg;
  Query message { static_cast<query_subject>(subject) };

  auto payload = to_bytes(message);
  sendMessage(std::move(payload), (size_t)Type::req_query);
}

void MessageHandler::send_add_plugin(size_t id, int index)
{
  using namespace msg;
  AddPlugin message {id, index};

  auto payload = to_bytes(message);
  sendMessage(std::move(payload), (size_t)Type::req_add_plugin);
}

void MessageHandler::send_bypass(size_t id, bool bypass)
{
  using namespace msg;
  BypassPlugin message {id, bypass};

  auto payload = to_bytes(message);
  sendMessage(std::move(payload), (size_t)Type::req_bypass_plugin);
}

void MessageHandler::send_remove_plugin(size_t id)
{
  msg::RemovePlugin message { id };
  auto payload = to_bytes(message);
  sendMessage(std::move(payload), (size_t)msg::Type::req_move_plugin);
}

void MessageHandler::send_move_plugin(size_t id, int index)
{
  msg::MovePlugin message {id, index};
  auto payload = to_bytes(message);
  sendMessage(std::move(payload), (size_t)msg::Type::req_move_plugin);
}

void MessageHandler::send_open_gui(size_t id)
{
  msg::OpenGui message { id };
  auto payload = to_bytes(message);
  sendMessage(std::move(payload), (size_t)msg::Type::req_open_gui);
}

void MessageHandler::send_rescan()
{
  msg::Rescan message {};
  auto payload = to_bytes(message);
  sendMessage(std::move(payload), (size_t)msg::Type::req_rescan);
}

void MessageHandler::send_add_path(const std::string& path)
{
  msg::AddPath message { path };
  auto payload = to_bytes(message);
  sendMessage(std::move(payload), (size_t)msg::Type::req_add_path);
}

void MessageHandler::send_remove_path(const std::string& path)
{
  msg::RemovePath message { path };
  auto payload = to_bytes(message);
  sendMessage(std::move(payload), (size_t)msg::Type::req_remove_path);
}

void MessageHandler::send_select_device(const std::string& id)
{
  msg::SelectDevice message { id };
  auto payload = to_bytes(message);
  sendMessage(std::move(payload), (size_t)msg::Type::req_select_device);
}

void MessageHandler::send_save_snapshot(const std::string& path)
{
  msg::SaveSnapshot message { path };
  auto payload = to_bytes(message);
  sendMessage(std::move(payload), (size_t)msg::Type::req_save_snapshot);
}

void MessageHandler::send_load_snapshot(const std::string& path)
{
  msg::LoadSnapshot message { path };
  auto payload = to_bytes(message);
  sendMessage(std::move(payload), (size_t)msg::Type::req_load_snapshot);
}

// MARK: handlers

void MessageHandler::handle_noop(size_t id, const std::vector<uint8_t>& pl)
{
  (void) id; (void) pl;
  std::cout << "Noop" << std::endl;
}

void MessageHandler::handle_completed(size_t id, const std::vector<uint8_t>& pl)
{
  (void) id; (void) pl;
  std::cout << "Ok" << std::endl;
}

void MessageHandler::handle_error(size_t id, const std::vector<uint8_t>& pl)
{
  using namespace msg;
  (void) id;
  Error message = from_bytes<Error>(pl);
  std::cout << "Error: " << message.message << std::endl;
}

void MessageHandler::handle_list_plugins(size_t id, const std::vector<uint8_t>& pl)
{
  using namespace msg;
  (void) id;
  ListPlugins message = from_bytes<ListPlugins>(pl);
  std::cout << "Plugins:";
  for(const auto& plugin : message.plugins)
  {
    std::cout << "\n  - " << plugin.plugin_name
              << "\n    " << plugin.id
              << "\n    " << plugin.category
              << "\n    " << plugin.manufacturer;
  }
  std::cout << "\n" << std::endl;
}

void MessageHandler::handle_list_chain(size_t id, const std::vector<uint8_t>& pl)
{
  using namespace msg;
  (void) id;
  ListChain message = from_bytes<ListChain>(pl);
  std::cout << "Active chain:";
  for(const auto& plugin : message.plugins)
  {
    std::cout << "\n  - " << plugin.display_name
              << "\n    " << plugin.id
              << "\n    " << (plugin.bypassed ? "bypassed" : "active");
  }
  std::cout << "\n" << std::endl;
}

void MessageHandler::handle_list_devices(size_t id, const std::vector<uint8_t>& pl)
{
  using namespace msg;
  (void) id;
  ListDevices message = from_bytes<ListDevices>(pl);
  std::cout << "Devices:";
  for(const auto& dev : message.devices)
  {
    std::cout << "\n  - " << dev.name
              << "\n    " << dev.id;
  }
  std::cout << "\n" << std::endl;
}

void MessageHandler::handle_failed_scans(size_t id, const std::vector<uint8_t>& pl)
{
  using namespace msg;
  (void) id;
  FailedScans message = from_bytes<FailedScans>(pl);
  std::cout << "Plugins with errors:";
  for(const auto& plugin : message.plugins)
  {
    std::cout << "\n  - " << plugin;
  }
  std::cout << "\n" << std::endl;
}

} // mitmegy
