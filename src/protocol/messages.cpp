#include "messages.hpp"

#include <cassert>
#include <algorithm>
#include <type_traits>
#include <functional>
#include <concepts>

namespace mitmegy::msg
{

namespace
{

using Iter = std::vector<uint8_t>::iterator;
using ConstIter = std::vector<uint8_t>::const_iterator;

constexpr int WIRE_TRUE = 1;
constexpr int WIRE_FALSE = 0;

template<typename type>
concept Fundamental = std::is_trivially_constructible_v<type>;

void checkSize(const Iter& begin, const Iter& end, size_t length)
{
  assert(begin <= end);
  const size_t space_left = end - begin;
  
  if(space_left < length)
    throw protocol_error(
      "Not enough space in bytestream");
}

void checkSize(const ConstIter& begin, const ConstIter& end, size_t length)
{
  assert(begin <= end);
  const size_t space_left = end - begin;
  
  if(space_left < length)
    throw protocol_error(
      "Not enough space in const bytestream");
}

// Deserialise, move begin to new read position
template<typename type>
type read(ConstIter& begin, const ConstIter& end)
{ throw protocol_error("Not implemented"); }

template<typename type>
size_t serialized_size(const type& val)
{ throw protocol_error("Not implemented"); }

// Serialise into byte stream, move begin to new write position
template<typename type>
void write(Iter& begin, const Iter& end, const type& val)
{ throw protocol_error("Not implemented"); }

template<>
std::string read<std::string>(ConstIter& begin, const ConstIter& end)
{
  assert(begin <= end);

  if(begin == end)
    return std::string();

  const auto term = std::find(begin, end, '\0');
  if(term == end)
    throw protocol_error(
      "Byte stream does not contain a terminator byte");

  const std::string str = std::string(begin, term);
  begin = term + 1;
  return str;
}

template<>
size_t serialized_size<std::string>(const std::string& str)
{
  return str.size() + 1;
}

template<>
void write<std::string>(Iter& begin, const Iter& end, const std::string& str)
{
  const size_t strlen = str.size();
  checkSize(begin, end, strlen + 1);

  if(str.find('\0') != std::string::npos)
    throw protocol_error(
      "String cannot contain a '\\0' character");

  std::memcpy(&*begin, str.data(), strlen);
  *(begin += strlen) = '\0';
  begin++;
}

template<typename type>
requires Fundamental<type>
type read(ConstIter& begin, const ConstIter& end)
{
  constexpr size_t size = sizeof(type);
  checkSize(begin, end, size);

  type value;
  std::memcpy(&value, &*begin, size);
  begin += size;
  return value;
}

template<typename type>
requires Fundamental<type>
size_t serialized_size(const type&)
{ return sizeof(type); }

template<typename type>
requires Fundamental<type>
void write(Iter& begin, const Iter& end, type value)
{
  constexpr size_t size = serialized_size<type>(value);
  checkSize(begin, end, size);

  std::memcpy(&*begin, &value, size);
  begin += size;
}

template<typename type>
std::vector<type> readVectorOf(
  ConstIter& begin, const ConstIter& end,
  std::function<type(ConstIter&, const ConstIter&)> deserializer 
    = [](ConstIter& begin, const ConstIter& end) -> type
    { return read<type>(begin, end); }
)
{
  ConstIter begin = bytes.begin();
  const size_t num_elements = read<size_t>(begin, bytes.end());
  std::vector<type> vec(num_elements);

  for(int i = 0; i < num_elements; i++)
    vec[i] = deserializer(begin, bytes.end());

  return vec;
}

template<typename type>
size_t serialized_vector_size(const std::vector<type>& vec)
{
  size_t size = sizeof(size_t); // num_elements
  for(const auto& elem : vec)
    size += size_getter(elem);

  return size;
}

template<typename type> 
void writeVectorOf(
  Iter& begin, const Iter& end, const std::vector<type>& vec,
  std::function<size_t(const type&)> size_getter
    = [](const type& val) -> size_t { return serialized_size<type>(val); },
  std::function<void(Iter&, const Iter& end, const type&)> serializer 
    = [](Iter& begin, const Iter& end, const type& value)
    { write<type>(begin, end, value); }
)
{
  size_t size = serialized_vector_size<type>(vec);
  checkSize(begin, end, size);

  Iter begin = bytes.begin();
  write<size_t>(begin, end, vec.size());
  for(const auto& elem : vec)
    write<type>(begin, end, elem);
}

// types

template<>
void write<plugin_runtime_desc>(Iter& begin, const Iter& end, const plugin_runtime_desc& val)
{
  return; 
}

template<> size_t serialized_size<plugin_runtime_desc>(const plugin_runtime_desc& val)
{

}

template<>
plugin_runtime_desc read(ConstIter& begin, const ConstIter& end)
{

}

template<>
void write<plugin_full_desc>(Iter& begin, const Iter& end, const plugin_full_desc& val)
{

}

template<> size_t serialized_size<plugin_full_desc>(const plugin_full_desc& val)
{

}

template<>
plugin_full_desc read<plugin_full_desc>(ConstIter& begin, const ConstIter& end)
{

}

template<>
void write<audio_device_desc>(Iter& begin, const Iter& end, const audio_device_desc& val)
{

}

template<> size_t serialized_size<audio_device_desc>(const audio_device_desc& val)
{

}

template<> audio_device_desc read<audio_device_desc>(ConstIter& begin, const ConstIter& end)
{
  
}

} // namespace

// messages

template<> std::vector<uint8_t> to_bytes<Noop>(const Noop& message)
{
  return std::vector<uint8_t> {};
}

template<> Noop from_bytes(const std::vector<uint8_t>& bytes)
{
  assert(bytes.size() == 0);
  return Noop {};
}

template<> std::vector<uint8_t> to_bytes<Path>(const Path& message)
{
  std::vector<uint8_t> bytes(serialized_size<std::string>(message.path));
  write<std::string>(bytes.begin(), bytes.end(), message.path);
  return bytes;
}

template<> Path from_bytes(const std::vector<uint8_t>& bytes)
{
  Path message;
  ConstIter front = bytes.begin();
  message.path = read<std::string>(front, bytes.end());
  return message;
}

template<> std::vector<uint8_t> to_bytes<Error>(const Error& message)
{
  std::vector<uint8_t> bytes(serialized_size<std::string>(message.message));
  write<std::string>(bytes.begin(), bytes.end(), message.message);
  return bytes;
}

template<> Error from_bytes(const std::vector<uint8_t>& bytes)
{
  Error message;
  ConstIter front = bytes.begin();
  message.message = read<std::string>(front, bytes.end());
  return message;
}

template<> std::vector<uint8_t> to_bytes<Query>(const Query& message)
{
  using Base = std::underlying_type_t<query_subject>;
  std::vector<uint8_t> bytes(sizeof(Base));
  write<Base>(bytes.begin(), bytes.end(), (Base)message.subject);
}

template<> Query from_bytes(const std::vector<uint8_t>& bytes)
{
  using Base = std::underlying_type_t<query_subject>;
  Base raw_value = read<Base>(bytes.begin(), bytes.end());
  return {(query_subject)(raw_value)};
}

template<> std::vector<uint8_t> to_bytes<ListPlugins>(const ListPlugins& message)
{
  std::vector<uint8_t> bytes(serialized_size(message));
  writeVectorOf<plugin_full_desc>(bytes.begin(), bytes.end(), message.plugins);
  return bytes;
}

template<> ListPlugins from_bytes(const std::vector<uint8_t>& bytes)
{
  ListPlugins list;
  list.plugins = readVectorOf<plugin_full_desc>(bytes.begin(), bytes.end());
  return list;
}

template<> std::vector<uint8_t> to_bytes<ListChain>(const ListChain& message)
{
  std::vector<uint8_t> bytes(serialized_size(message));
  writeVectorOf<plugin_runtime_desc>(bytes.begin(), bytes.end(), message.plugins);
  return bytes;
}

template<> ListChain from_bytes(const std::vector<uint8_t>& bytes)
{
  ListChain list;
  list.plugins = readVectorOf<plugin_runtime_desc>(bytes.begin(), bytes.end());
  return list;
}

template<> std::vector<uint8_t> to_bytes<ListDevices>(const ListDevices& message)
{
  std::vector<uint8_t> bytes(serialized_size(message));
  writeVectorOf<audio_device_desc>(bytes.begin(), bytes.end(), message.devices);
  return bytes;
}

template<> ListDevices from_bytes(const std::vector<uint8_t>& bytes)
{
  ListDevices list;
  list.devices = readVectorOf<audio_device_desc>(bytes.begin(), bytes.end());
  return list;
}

template<> std::vector<uint8_t> to_bytes<ListPaths>(const ListPaths& message)
{
  std::vector<uint8_t> bytes(serialized_size(message));
  writeVectorOf<std::string>(bytes.begin(), bytes.end(), message.paths);
  return bytes;
}

template<> ListPaths from_bytes(const std::vector<uint8_t>& bytes)
{
  ListPaths list;
  list.paths = readVectorOf<std::string>(bytes.begin(), bytes.end());
  return list;
}

template<> std::vector<uint8_t> to_bytes<AddPlugin>(const AddPlugin& message)
{
  std::vector<uint8_t> bytes(sizeof(plugin_static_id) + sizeof(int));

  Iter begin = bytes.begin();
  write<plugin_static_id>(begin, bytes.end(), message.id);
  write<int>(begin, bytes.end(), message.index);
  return bytes;
}

template<> AddPlugin from_bytes(const std::vector<uint8_t>& bytes)
{
  AddPlugin message;
  ConstIter begin = bytes.begin();
  message.id = read<plugin_static_id>(begin, bytes.end());
  message.index = read<int>(begin, bytes.end());
  return message;
}

template<> std::vector<uint8_t> to_bytes<BypassPlugin>(const BypassPlugin& message)
{
  std::vector<uint8_t> bytes(sizeof(plugin_runtime_id) + sizeof(int));
  const int bypassed = message.bypass ? WIRE_TRUE : WIRE_FALSE;

  Iter begin = bytes.begin();
  write<plugin_runtime_id>(begin, bytes.end(), message.bypass);
  write<int>(begin, bytes.end(), bypassed);
  return bytes;
}

template<> BypassPlugin from_bytes(const std::vector<uint8_t>& bytes)
{
  BypassPlugin message;
  ConstIter begin = bytes.begin();
  message.id = read<plugin_runtime_id>(begin, bytes.end());
  const int bypass = read<int>(begin, bytes.end());
  if(bypass == WIRE_TRUE)
    message.bypass = true;
  else if (bypass == WIRE_FALSE)
    message.bypass = false;
  else
    throw protocol_error("Invalid boolean in BypassPlugin from_bytes");

  return message;
}

template<> std::vector<uint8_t> to_bytes<MovePlugin>(const MovePlugin& message)
{
  std::vector<uint8_t> bytes(sizeof(plugin_runtime_id) + sizeof(int));

  Iter begin = bytes.begin();
  write<plugin_runtime_id>(begin, bytes.end(), message.id);
  write<int>(begin, bytes.end(), message.index);
  return bytes;
}

template<> MovePlugin from_bytes(const std::vector<uint8_t>& bytes)
{
  MovePlugin message;
  ConstIter front = bytes.begin();
  message.id = read<plugin_runtime_id>(front, bytes.end());
  message.index = read<int>(front, bytes.end());
  return message;
}

template<> std::vector<uint8_t> to_bytes<SimpleId>(const SimpleId& message)
{
  std::vector<uint8_t> bytes(sizeof(size_t));

  Iter begin = bytes.begin();
  write<size_t>(begin, bytes.end(), message.id);
  return bytes;
}

template<> SimpleId from_bytes(const std::vector<uint8_t>& bytes)
{
  return { read<size_t>(bytes.begin(), bytes.end()) };
}

} // namespace mitmegy::msg
