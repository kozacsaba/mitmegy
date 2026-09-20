#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <stdexcept>

namespace mitmegy
{

namespace msg
{

enum class Type : size_t
{
  noop = 0, // do nothing
  res_completed, // srv -> ctl : request is completed with no response msg
  res_error, // srv -> ctl : could not complete request

  req_query,
  res_query_plugins,
  res_query_chain,
  res_query_devices,

  req_add_plugin,
  req_bypass_plugin,
  req_remove_plugin,
  req_move_plugin, // in chain sequence

  req_open_gui, // open editor for plugin
  req_rescan, // rescan plugiin database

  req_add_path, // add plugin scan path
  req_remove_path, // remove plugin scan path

  req_select_device, // select playback / output device

  req_save_snapshot, // save current plugin chain
  req_load_snapshot, // load a saved plugin chain
};

// MARK: Types

struct protocol_error : public std::runtime_error
{ using std::runtime_error::runtime_error; };

enum class query_subject : size_t
{
  null = 0, // query nothing, probably a bug
  plugin_list,
  active_chain,
  audio_devices,
  search_paths,
};

// ids do not need to persist between launches because they do not get saved
using plugin_static_id = size_t;
using plugin_runtime_id = size_t;
using audio_device_id = size_t;

struct plugin_runtime_desc // full description of a plugin that the gui could need 
{
  plugin_runtime_id id;
  std::string display_name;
  bool bypassed;
};

struct plugin_full_desc
{
  plugin_static_id id;
  std::string plugin_name;
  std::string manufacturer;
  std::string category;
};

struct audio_device_desc
{
  audio_device_id id;
  std::string type; // not sure about this, just guessing at this point
  std::string name;
};

// MARK: Payloads

struct Noop {};

using Completed = Noop;
using Rescan = Noop;

struct Path
{
  std::string path;
};

using AddPath = Path;
using RemovePath = Path;
using SaveSnapshot = Path;
using LoadSnapshot = Path;

struct Error
{
  std::string message;
};

struct Query
{
  query_subject subject;
};

struct ListPlugins
{
  std::vector<plugin_full_desc> plugins;
};

struct ListChain
{
  std::vector<plugin_runtime_desc> plugins;
};

struct ListDevices
{
  std::vector<audio_device_desc> devices;
};

struct ListPaths
{
  std::vector<std::string> paths;
};

struct AddPlugin
{
  plugin_static_id id;
  int index; // -1 for append at end
};

struct BypassPlugin
{
  plugin_runtime_id id;
  bool bypass;
};

struct MovePlugin
{
  plugin_runtime_id id;
  int index;
};

struct SimpleId
{
  size_t id;
};

using RemovePlugin = SimpleId;
using OpenGui = SimpleId;
using SelectDevice = SimpleId;

// MARK: Serialisation

template<typename type>
std::vector<uint8_t> to_bytes(const type&)
{ throw protocol_error("Not implemented"); }

template<typename type>
type from_bytes(const std::vector<uint8_t>&)
{ throw protocol_error("Not implemented"); }

template<> std::vector<uint8_t> to_bytes<Noop>(const Noop& message);
template<> std::vector<uint8_t> to_bytes<Path>(const Path& message);
template<> std::vector<uint8_t> to_bytes<Error>(const Error& message);
template<> std::vector<uint8_t> to_bytes<Query>(const Query& message);
template<> std::vector<uint8_t> to_bytes<ListPlugins>(const ListPlugins& message);
template<> std::vector<uint8_t> to_bytes<ListChain>(const ListChain& message);
template<> std::vector<uint8_t> to_bytes<ListDevices>(const ListDevices& message);
template<> std::vector<uint8_t> to_bytes<ListPaths>(const ListPaths& message);
template<> std::vector<uint8_t> to_bytes<AddPlugin>(const AddPlugin& message);
template<> std::vector<uint8_t> to_bytes<BypassPlugin>(const BypassPlugin& message);
template<> std::vector<uint8_t> to_bytes<MovePlugin>(const MovePlugin& message);
template<> std::vector<uint8_t> to_bytes<SimpleId>(const SimpleId& message);

template<> Noop from_bytes(const std::vector<uint8_t>& bytes);
template<> Path from_bytes(const std::vector<uint8_t>& bytes);
template<> Error from_bytes(const std::vector<uint8_t>& bytes);
template<> Query from_bytes(const std::vector<uint8_t>& bytes);
template<> ListPlugins from_bytes(const std::vector<uint8_t>& bytes);
template<> ListChain from_bytes(const std::vector<uint8_t>& bytes);
template<> ListDevices from_bytes(const std::vector<uint8_t>& bytes);
template<> ListPaths from_bytes(const std::vector<uint8_t>& bytes);
template<> AddPlugin from_bytes(const std::vector<uint8_t>& bytes);
template<> BypassPlugin from_bytes(const std::vector<uint8_t>& bytes);
template<> MovePlugin from_bytes(const std::vector<uint8_t>& bytes);
template<> SimpleId from_bytes(const std::vector<uint8_t>& bytes);

} // namespace msg

} // namespace mitmegy
