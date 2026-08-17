#pragma once

#include <atomic>
#include <cstddef>
#include <span>
#include <vector>

namespace mitmegy
{

class AudioFifo
{
public:
  class ReadView;
  class WriteView;

  AudioFifo(size_t channels, size_t capacity_frames);
  AudioFifo(const AudioFifo&) = delete;
  AudioFifo& operator=(const AudioFifo&) = delete;

  size_t channels() const;
  size_t capacityFrames() const;
  size_t readableFrames() const;
  size_t writableFrames() const;

  WriteView acquireWrite();
  ReadView acquireRead();

  size_t writeInterleaved(std::span<const float> source, size_t frames);
  size_t readInterleaved(std::span<float> destination, size_t frames);

  void reset();

private:
  void commitWrite(size_t frames);
  void consumeRead(size_t frames);

  void cancelWrite();
  void cancelRead();

  float* channelPointer(size_t channel, size_t offset);
  const float* channelPointer(size_t channel, size_t offset) const;

  size_t channels_;
  size_t capacity_frames_;

  std::vector<float> data_;

  // Monotonically increasing frame positions.
  // Modulo capacity_frames_ gives the physical ring offset.
  std::atomic<size_t> read_position_{0};
  std::atomic<size_t> write_position_{0};

  // SPSC: each flag is only touched by its owning side.
  bool read_view_active_ = false;
  bool write_view_active_ = false;
};

class AudioFifo::WriteView
{
public:
  WriteView() = default;
  ~WriteView();
  WriteView(const WriteView&) = delete;
  WriteView& operator=(const WriteView&) = delete;
  WriteView(WriteView&& other) noexcept;
  WriteView& operator=(WriteView&& other) noexcept;

  explicit operator bool() const;
  size_t frames() const;
  size_t channels() const;
  std::span<float> channel(size_t channel);
  void commit(size_t frames);

private:
  friend class AudioFifo;

  WriteView(AudioFifo* fifo, size_t offset, size_t frames);
  void cancel();

  AudioFifo* fifo_ = nullptr;
  size_t offset_ = 0;
  size_t frames_ = 0;
};

class AudioFifo::ReadView
{
public:
  ReadView() = default;
  ~ReadView();
  ReadView(const ReadView&) = delete;
  ReadView& operator=(const ReadView&) = delete;
  ReadView(ReadView&& other) noexcept;
  ReadView& operator=(ReadView&& other) noexcept;

  explicit operator bool() const;
  size_t frames() const;
  size_t channels() const;
  std::span<const float> channel(size_t channel) const;
  void consume(size_t frames);

private:
  friend class AudioFifo;

  ReadView(AudioFifo* fifo, size_t offset, size_t frames);
  void cancel();

  AudioFifo* fifo_ = nullptr;
  size_t offset_ = 0;
  size_t frames_ = 0;
};

} // namespace mitmegy
