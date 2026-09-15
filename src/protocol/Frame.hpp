#pragma once

#include <cstdint>
#include <vector>
#include <optional>

namespace mitmegy
{

constexpr size_t VERSION = 0;

struct Frame
{
  struct Header
  {
    size_t version = VERSION;
    size_t msg_type;
    size_t length;
  } header;
  std::vector<uint8_t> payload;
}; // Frame

std::vector<uint8_t> serialiseFrame(const Frame& frame);

class Framer
{
public:
  void pushBytes(const std::vector<uint8_t>& bytes);

  // Not thread safe. Should be called form the same thread as pushBytes
  std::optional<Frame> getFrame();

private:
  std::vector<uint8_t> buffer_;
  Frame stage_;
  size_t target_bytes_;

}; // Framer

} // namespace mitmegy
