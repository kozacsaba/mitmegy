#include "AudioEngine.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <stdexcept>
#include <thread>
#include <utility>

namespace mitmegy
{

void TransportStats::observeFifo(const AudioFifo& fifo)
{
  const size_t fill = fifo.readableFrames();

  fifo_fill = fill;
  fifo_min_fill = std::min(fifo_min_fill, fill);
  fifo_max_fill = std::max(fifo_max_fill, fill);
}

AudioEngine::AudioEngine(const Endpoint& audio_sink, const Endpoint& output, ProcessCallback process)
  : capture_(audio_sink)
  , fifo_(capture_.channels(), capture_.sampleRate())
  , render_(output, capture_.format())
  , process_(std::move(process))
{
  if (capture_.sampleFormat() != SampleFormat::Float32)
    throw std::runtime_error("Current engine only supports 32-bit float AudioSink input");

  if (capture_.format().nBlockAlign != capture_.format().nChannels * sizeof(float))
    throw std::runtime_error("Unexpected AudioSink float frame layout");
}

const TransportStats& AudioEngine::stats() const
{
  return stats_;
}

const WAVEFORMATEX& AudioEngine::inputFormat() const
{
  return capture_.format();
}

SampleFormat AudioEngine::inputSampleFormat() const
{
  return capture_.sampleFormat();
}

void AudioEngine::capturePacket()
{
  const CaptureResult result = capture_.captureInto(fifo_);
  stats_.captured_frames += result.captured_frames;

  if (result.discontinuity)
    ++stats_.capture_discontinuities;

  if (result.written_frames < result.captured_frames)
  {
    ++stats_.overruns;
    stats_.dropped_frames += result.captured_frames - result.written_frames;
  }

  stats_.observeFifo(fifo_);
}

void AudioEngine::prefill(const std::atomic_bool& running)
{
  constexpr size_t prefill_ms = 50;

  const size_t target_frames = capture_.sampleRate() * prefill_ms / 1000;

  while (running.load(std::memory_order_relaxed) && fifo_.readableFrames() < target_frames)
  {
    size_t packet_frames = capture_.nextPacketFrames();

    if (packet_frames == 0)
    {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
      continue;
    }

    while (packet_frames > 0 && fifo_.readableFrames() < target_frames)
    {
      capturePacket();
      packet_frames = capture_.nextPacketFrames();
    }
  }
}

void AudioEngine::guardOutput(AudioFifo::ReadView& view, size_t frames)
{
  bool invalid = false;
  for (size_t channel = 0; channel < view.channels(); ++channel)
  {
    const auto samples = view.channel(channel);
    for (size_t frame = 0; frame < frames; ++frame)
    {
      if (!std::isfinite(samples[frame]))
      {
        invalid = true;
        break;
      }
    }

    if (invalid)
      break;
  }

  if (!invalid)
    return;

  for (size_t channel = 0; channel < view.channels(); ++channel)
  {
    auto samples = view.channel(channel);
    std::fill_n(samples.begin(), frames, 0.0f);
  }
}

bool AudioEngine::renderFromFifo()
{
  const size_t padding = render_.paddingFrames();
  if (padding == 0 && fifo_.readableFrames() == 0)
  {
    ++stats_.underruns;
    return false;
  }

  const size_t available_frames = render_.bufferFrames() - padding;
  if (available_frames == 0)
    return false;

  auto view = fifo_.acquireRead();
  if (!view)
    return false;

  const size_t frames = std::min(view.frames(), available_frames);

  if (process_)
    process_(view, frames);
  guardOutput(view, frames);
  render_.write(view, frames);

  view.consume(frames);
  stats_.rendered_frames += frames;
  stats_.observeFifo(fifo_);

  return true;
}

void AudioEngine::runLoop(const std::atomic_bool& running)
{
  while (running.load(std::memory_order_relaxed))
  {
    bool did_work = false;

    size_t packet_frames = capture_.nextPacketFrames();
    while (packet_frames > 0)
    {
      capturePacket();
      did_work = true;
      packet_frames = capture_.nextPacketFrames();
    }

    if (renderFromFifo())
      did_work = true;

    if (!did_work)
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
}

void AudioEngine::run(const std::atomic_bool& running)
{
  fifo_.reset();

  stats_ = {};
  stats_.observeFifo(fifo_);

  capture_.start();

  try
  {
    prefill(running);
    if (running.load(std::memory_order_relaxed))
    {
      // Prime the render buffer before its clock starts.
      renderFromFifo();
      render_.start();
      runLoop(running);
    }
  }
  catch (...)
  {
    render_.stop();
    capture_.stop();
    throw;
  }

  render_.stop();
  capture_.stop();
}

} // namespace mitmegy
