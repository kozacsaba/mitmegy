#include "Frame.hpp"

namespace mitmegy
{

constexpr size_t HeaderSize = sizeof(Frame::Header);

std::vector<uint8_t> serialiseFrame(const Frame& frame)
{
  std::vector<uint8_t> byte_vector;
  byte_vector.resize(HeaderSize + frame.payload.size());
  std::memcpy(byte_vector.data(), &frame.header, HeaderSize);
  std::memcpy(byte_vector.data() + HeaderSize, frame.payload.data(), frame.payload.size());
  return byte_vector;
}

void Framer::pushBytes(const std::vector<uint8_t>& bytes)
{
  buffer_.insert(buffer_.end(), bytes.begin(), bytes.end());
}

std::optional<Frame> Framer::getFrame()
{
  // practically empty buffer
  if(buffer_.size() < HeaderSize)
    return std::nullopt;

  // just got some data
  if(target_bytes_ == 0)
  {
    std::memcpy(&stage_.header, buffer_.data(), HeaderSize);
    target_bytes_ = HeaderSize + stage_.header.length;
  }

  // building staged frame
  if(buffer_.size() < target_bytes_)
    return std::nullopt;

  // frame ready
  stage_.payload.clear();
  if(target_bytes_ > HeaderSize)
    stage_.payload.insert(
      stage_.payload.begin(),
      buffer_.begin() + HeaderSize,
      buffer_.begin() + target_bytes_);

  buffer_.erase(buffer_.begin(), buffer_.begin() + target_bytes_);
  target_bytes_ = 0;
  return std::move(stage_);
}

} // namespace mitmegy
