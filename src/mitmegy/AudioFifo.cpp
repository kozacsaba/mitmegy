#include "AudioFifo.hpp"

#include <algorithm>
#include <cassert>
#include <stdexcept>
#include <utility>

namespace mitmegy
{

AudioFifo::AudioFifo(size_t channels, size_t capacity_frames)
  : channels_(channels)
  , capacity_frames_(capacity_frames)
  , data_(channels * capacity_frames)
{
  if (channels == 0)
    throw std::invalid_argument("AudioFifo must have at least one channel");

  if (capacity_frames == 0)
    throw std::invalid_argument("AudioFifo capacity must be greater than zero");
}

size_t AudioFifo::channels() const
{
  return channels_;
}

size_t AudioFifo::capacityFrames() const
{
  return capacity_frames_;
}

size_t AudioFifo::readableFrames() const
{
  const size_t write = write_position_.load(std::memory_order_acquire);
  const size_t read = read_position_.load(std::memory_order_acquire);
  return write - read;
}

size_t AudioFifo::writableFrames() const
{
  return capacity_frames_ - readableFrames();
}

AudioFifo::WriteView AudioFifo::acquireWrite()
{
  assert(!write_view_active_);

  const size_t write = write_position_.load(std::memory_order_relaxed);
  const size_t read = read_position_.load(std::memory_order_acquire);
  
  const size_t writable = capacity_frames_ - (write - read);
  const size_t offset = write % capacity_frames_;
  const size_t contiguous = capacity_frames_ - offset;
  const size_t frames = std::min({writable, contiguous});

  if (frames == 0)
    return {};

  write_view_active_ = true;

  return WriteView(this, offset, frames);
}

AudioFifo::ReadView AudioFifo::acquireRead()
{
  assert(!read_view_active_);

  const size_t read = read_position_.load(std::memory_order_relaxed);
  const size_t write = write_position_.load(std::memory_order_acquire);

  const size_t readable = write - read;
  const size_t offset = read % capacity_frames_;
  const size_t contiguous = capacity_frames_ - offset;
  const size_t frames = std::min({readable, contiguous});

  if (frames == 0)
    return {};

  read_view_active_ = true;

  return ReadView(this, offset, frames);
}

size_t AudioFifo::writeInterleaved(std::span<const float> src, size_t frames)
{
  assert(src.size() >= frames * channels_);

  size_t written = 0;
  while (written < frames)
  {
    auto view = acquireWrite();
    if (!view) break;

    const size_t chunk = std::min(view.frames(), frames - written);
    for (size_t channel = 0; channel < channels_; ++channel)
    {
      auto destination = view.channel(channel);

      for (size_t frame = 0; frame < chunk; ++frame)
        destination[frame] = src[(written + frame) * channels_ + channel];
    }

    view.commit(chunk);
    written += chunk;
  }

  return written;
}

size_t AudioFifo::readInterleaved(std::span<float> dst, size_t frames)
{
  assert(dst.size() >= frames * channels_);

  size_t read = 0;
  while (read < frames)
  {
    auto view = acquireRead();
    if (!view) break;

    const size_t chunk = std::min(view.frames(), frames - read);
    for (size_t channel = 0; channel < channels_; ++channel)
    {
      auto source = view.channel(channel);

      for (size_t frame = 0; frame < chunk; ++frame)
        dst[(read + frame) * channels_ + channel] = source[frame];
    }

    view.consume(chunk);
    read += chunk;
  }

  return read;
}

void AudioFifo::reset()
{
  assert(!read_view_active_);
  assert(!write_view_active_);

  read_position_.store(0, std::memory_order_relaxed);
  write_position_.store(0, std::memory_order_relaxed);
}

void AudioFifo::commitWrite(size_t frames)
{
  assert(write_view_active_);

  const size_t write = write_position_.load(std::memory_order_relaxed);
  write_position_.store(write + frames, std::memory_order_release);
  write_view_active_ = false;
}

void AudioFifo::consumeRead(size_t frames)
{
  assert(read_view_active_);

  const size_t read = read_position_.load(std::memory_order_relaxed);
  read_position_.store(read + frames, std::memory_order_release);
  read_view_active_ = false;
}

void AudioFifo::cancelWrite()
{
  assert(write_view_active_);
  write_view_active_ = false;
}

void AudioFifo::cancelRead()
{
  assert(read_view_active_);
  read_view_active_ = false;
}

float* AudioFifo::channelPointer(size_t channel, size_t offset)
{
  assert(channel < channels_);
  assert(offset < capacity_frames_);

  return data_.data()
    + channel * capacity_frames_
    + offset;
}

const float* AudioFifo::channelPointer(size_t channel, size_t offset) const
{
  assert(channel < channels_);
  assert(offset < capacity_frames_);

  return data_.data()
    + channel * capacity_frames_
    + offset;
}

// WriteView

AudioFifo::WriteView::WriteView(AudioFifo* fifo, size_t offset, size_t frames)
  : fifo_(fifo)
  , offset_(offset)
  , frames_(frames)
{}

AudioFifo::WriteView::~WriteView()
{
  cancel();
}

AudioFifo::WriteView::WriteView(WriteView&& other) noexcept
  : fifo_(std::exchange(other.fifo_, nullptr))
  , offset_(other.offset_)
  , frames_(std::exchange(other.frames_, 0))
{}

AudioFifo::WriteView& AudioFifo::WriteView::operator=(WriteView&& other) noexcept
{
  if (this == &other)
    return *this;

  cancel();

  fifo_ = std::exchange(other.fifo_, nullptr);
  offset_ = other.offset_;
  frames_ = std::exchange(other.frames_, 0);

  return *this;
}

AudioFifo::WriteView::operator bool() const
{
  return fifo_ != nullptr;
}

size_t AudioFifo::WriteView::frames() const
{
  return frames_;
}

size_t AudioFifo::WriteView::channels() const
{
  return fifo_ ? fifo_->channels() : 0;
}

std::span<float> AudioFifo::WriteView::channel(size_t channel)
{
  assert(fifo_ != nullptr);
  assert(channel < fifo_->channels());

  if (!fifo_) return {};

  return {fifo_->channelPointer(channel, offset_), frames_};
}

void AudioFifo::WriteView::commit(size_t frames)
{
  assert(fifo_ != nullptr);
  assert(frames <= frames_);

  if (!fifo_)
    return;

  fifo_->commitWrite(frames);

  fifo_ = nullptr;
  offset_ = 0;
  frames_ = 0;
}

void AudioFifo::WriteView::cancel()
{
  if (!fifo_)
    return;

  fifo_->cancelWrite();

  fifo_ = nullptr;
  offset_ = 0;
  frames_ = 0;
}

// ReadView

AudioFifo::ReadView::ReadView(AudioFifo* fifo, size_t offset, size_t frames)
  : fifo_(fifo)
  , offset_(offset)
  , frames_(frames)
{}

AudioFifo::ReadView::~ReadView()
{
  cancel();
}

AudioFifo::ReadView::ReadView(ReadView&& other) noexcept
  : fifo_(std::exchange(other.fifo_, nullptr))
  , offset_(other.offset_)
  , frames_(std::exchange(other.frames_, 0))
{}

AudioFifo::ReadView& AudioFifo::ReadView::operator=(ReadView&& other) noexcept
{
  if (this == &other)
    return *this;

  cancel();

  fifo_ = std::exchange(other.fifo_, nullptr);
  offset_ = other.offset_;
  frames_ = std::exchange(other.frames_, 0);

  return *this;
}

AudioFifo::ReadView::operator bool() const
{
  return fifo_ != nullptr;
}

size_t AudioFifo::ReadView::frames() const
{
  return frames_;
}

size_t AudioFifo::ReadView::channels() const
{
  return fifo_ ? fifo_->channels() : 0;
}

std::span<const float> AudioFifo::ReadView::channel(size_t channel) const
{
  assert(fifo_ != nullptr);
  assert(channel < fifo_->channels());

  if (!fifo_)
    return {};

  return {fifo_->channelPointer(channel, offset_), frames_};
}

void AudioFifo::ReadView::consume(size_t frames)
{
  assert(fifo_ != nullptr);
  assert(frames <= frames_);

  if (!fifo_)
    return;

  fifo_->consumeRead(frames);

  fifo_ = nullptr;
  offset_ = 0;
  frames_ = 0;
}

void AudioFifo::ReadView::cancel()
{
  if (!fifo_)
    return;

  fifo_->cancelRead();

  fifo_ = nullptr;
  offset_ = 0;
  frames_ = 0;
}

} // namespace mitmegy
