#include "MessageHandler.hpp"
#include "Engine.hpp"

#include "vsthost/PluginChain.hpp"
#include "vsthost/PluginRegistry.hpp"
#include "protocol/messages.hpp"

#ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
  #define NOMINMAX
#endif

#include <window.h>

#include <algorithm>
#include <type_traits>

static_assert(std::is_same_v<mitmegy::msg::plugin_runtime_id, mitmegy::PluginChain::PluginID>);

namespace
{

std::string narrow(std::wstring_view str)
{
  if (str.empty())
    return {};

  const int size = WideCharToMultiByte(
    CP_UTF8,
    0,
    str.data(),
    static_cast<int>(str.size()),
    nullptr,
    0,
    nullptr,
    nullptr);

  std::string result(size, '\0');

  WideCharToMultiByte(
    CP_UTF8,
    0,
    str.data(),
    static_cast<int>(str.size()),
    result.data(),
    size,
    nullptr,
    nullptr);

  return result;
}

} // namespace

namespace mitmegy
{

MessageHandler::MessageHandler(Engine& engine, PluginChain& chain, PluginRegistry& registry)
  : engine_(engine)
  , chain_(chain)
  , registry_(registry)
  , server_(PIPENAME)
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

  thread_ = std::thread(worker, this);
}

void MessageHandler::stop()
{
  if(!running_.exchange(false))
    return;

  server_.cancel();
  { // lock connection
    std::scoped_lock lock(connection_mut_);
    if(connection_)
      connection_->cancel();
  } // lock connetionc

  if(thread_.joinable())
    thread_.join();
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
      connection_ = server_.accept();
      if(!connection_)
        continue;
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

void MessageHandler::handle_noop(size_t id, const std::vector<uint8_t>& pl)
{
  (void) pl;
  sendSuccess(id);
}

void MessageHandler::handle_query(size_t id, const std::vector<uint8_t>& pl)
{
  using namespace msg;
  const Query request = from_bytes<Query>(pl);

  switch(request.subject)
  {
    case query_subject::active_chain:
    {
      const auto active_chain = chain_.getChain();

      ListChain response {};
      response.plugins.reserve(active_chain.size());
      for(const auto& plugin : active_chain)
      {
        plugin_runtime_desc desc;
        desc.id = plugin->runtime_id_;
        desc.display_name = plugin->IdentifierString_.toStdString(); // TODO: probably not what we want here
        desc.bypassed = plugin->bypassed_;
        response.plugins.push_back(std::move(desc));
      }

      auto payload = to_bytes(response);
      sendMessage(std::move(payload), (size_t)Type::res_query_chain, id);
      break;
    }
    case query_subject::audio_devices:
    {
      const auto& endpoints = engine_.getEndpoints();

      ListDevices message {};
      message.devices.reserve(endpoints.size());
      for(const auto& ep : endpoints)
      {
        audio_device_desc device;
        device.id = narrow(ep.id);
        device.name = narrow(ep.name);
        message.devices.push_back(std::move(device));
      }

      auto payload = to_bytes(message);
      sendMessage(std::move(payload), (size_t)Type::res_query_devices, id);
      break;
    }
    case query_subject::plugin_list:
    {
      // TODO: plugin list
      break;
    }
    case query_subject::search_paths:
    {
      // TODO: search paths
      break;
    }
    case query_subject::null:
    default:
      sendError(id, "Invalid query subject: " 
                    + std::to_string((size_t)request.subject));
  }
}

void MessageHandler::handle_add_plugin(size_t id, const std::vector<uint8_t>& pl)
{
  // TODO: implement
}

void MessageHandler::handle_bypass_plugin(size_t id, const std::vector<uint8_t>& pl)
{
  using namespace msg;
  const BypassPlugin request = from_bytes<BypassPlugin>(pl);

  auto ptr = chain_.plugin_id(request.id);
  if(ptr == nullptr)
  {
    sendError(id, "Plugin with given id does not exist");
    return;
  }

  ptr->bypassed_ = request.bypass;
  sendSuccess(id);
}

void MessageHandler::handle_remove_plugin(size_t id, const std::vector<uint8_t>& pl)
{
  using namespace msg;
  const RemovePlugin request = from_bytes<RemovePlugin>(pl);

  chain_.remove(request.id);
  sendSuccess(id);
}

void MessageHandler::handle_move_plugin(size_t id, const std::vector<uint8_t>& pl)
{
  // TODO: needs a move method in PluginChain
}

void MessageHandler::handle_open_gui(size_t id, const std::vector<uint8_t>& pl)
{
  // TODO: a lot...
}

void MessageHandler::handle_rescan        (size_t id, const std::vector<uint8_t>& pl)
{

}

void MessageHandler::handle_add_path      (size_t id, const std::vector<uint8_t>& pl)
{
  // TODO: no search paths just yet
}

void MessageHandler::handle_remove_path   (size_t id, const std::vector<uint8_t>& pl)
{
  // TODO: no search paths just yet
}

void MessageHandler::handle_select_device(size_t id, const std::vector<uint8_t>& pl)
{
  using namespace msg;
  const SelectDevice request = from_bytes<SelectDevice>(pl);

  const auto& endpoints = engine_.getEndpoints();
  auto it = std::find_if(
    endpoints.begin(), endpoints.end(), 
    [&](const Endpoint& ep)
    {
      return request.id == narrow(ep.id);
    });

  if(it == endpoints.end())
  {
    sendError(id, "Device with id is not available");
    return;
  }

  engine_.selectDevice(it->id);
  sendSuccess(id);
}

void MessageHandler::handle_save_snapshot (size_t id, const std::vector<uint8_t>& pl)
{
  // TODO: no snapshots yet
}

void MessageHandler::handle_load_snapshot (size_t id, const std::vector<uint8_t>& pl)
{
  // TODO: no snapshots yet
}

void MessageHandler::sendError(size_t id, const std::string& msg)
{
  using namespace msg;
  Error response;
  response.message = msg;
  auto payload = msg::to_bytes(response);
  sendMessage(std::move(payload), (size_t)Type::res_error, id);
}

void MessageHandler::sendSuccess(size_t id)
{
  using namespace msg; 
  Completed response;
  auto payload = to_bytes(response);
  sendMessage(std::move(payload), (size_t)Type::res_completed, id);
}

void MessageHandler::sendMessage(std::vector<uint8_t>&& payload, size_t type, size_t id)
{
  Frame frame;
  frame.payload = std::move(payload);
  frame.header.id = id;
  frame.header.length = frame.payload.size();
  frame.header.msg_type = type;
  frame.header.version = VERSION;

  const auto packet = serializeFrame(frame);
  connection_->writeAll(packet);
}

} // namespace mitmegy
