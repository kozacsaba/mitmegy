#pragma once

#include <cstdint>
#include <vector>
#include <optional>

namespace mitmegy
{

constexpr uint8_t V_MAJ = 0;
constexpr uint8_t V_MIN = 0;

struct Frame
{
  struct Header
  {
    const uint8_t major = V_MAJ;
    const uint8_t minor = V_MIN;
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
