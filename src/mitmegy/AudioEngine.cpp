#include "AudioEngine.hpp"

#include <system_error>

#include <algorithm>
#include <cmath>
#include <stdexcept>
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

AudioEngine::AudioEngine(const Endpoint& audio_sink, const Endpoint& output)
  : audio_sink_(audio_sink)
  , output_(output)
  , control_event_(createEvent())
  , capture_(std::make_unique<WasapiCapture>(audio_sink_))
  , fifo_(std::make_unique<AudioFifo>(capture_->channels(), capture_->sampleRate()))
  , render_(std::make_unique<WasapiRender>(output_, capture_->format()))
{
  if (capture_->sampleFormat() != SampleFormat::Float32)
    throw std::runtime_error("Current engine only supports 32-bit float AudioSink input");

  if (capture_->format().nBlockAlign != capture_->format().nChannels * sizeof(float))
    throw std::runtime_error("Unexpected AudioSink float frame layout");
}

const TransportStats& AudioEngine::stats() const
{
  return stats_;
}

const WAVEFORMATEX& AudioEngine::inputFormat() const
{
  return capture_->format();
}

SampleFormat AudioEngine::inputSampleFormat() const
{
  return capture_->sampleFormat();
}

size_t AudioEngine::maxBlockSize() const
{
  return render_->bufferFrames();
}

void AudioEngine::capturePacket()
{
  const CaptureResult result = capture_->captureInto(*fifo_);
  stats_.captured_frames += result.captured_frames;

  if (result.discontinuity)
    ++stats_.capture_discontinuities;

  if (result.written_frames < result.captured_frames)
  {
    ++stats_.overruns;
    stats_.dropped_frames += result.captured_frames - result.written_frames;
  }

  stats_.observeFifo(*fifo_);
}

void AudioEngine::prefill()
{
  constexpr size_t prefill_ms = 50;
  const size_t target_frames = capture_->sampleRate() * prefill_ms / 1000;
  const HANDLE events[] = {control_event_.get(), capture_->eventHandle()};

  while (!stop_requested_.load(std::memory_order_acquire) && 
         fifo_->readableFrames() < target_frames)
  {
    while (!stop_requested_.load(std::memory_order_acquire) &&
           fifo_->readableFrames() < target_frames &&
           capture_->nextPacketFrames() > 0)
    {
      capturePacket();
    }

    if (stop_requested_.load(std::memory_order_acquire) ||
        fifo_->readableFrames() >= target_frames)
      break;

    const DWORD result = ::WaitForMultipleObjects(2, events, FALSE, INFINITE);

    if (result == WAIT_OBJECT_0)
      return;

    if (result == WAIT_OBJECT_0 + 1)
      continue;

    if (result == WAIT_FAILED)
      throw std::system_error(static_cast<int>(::GetLastError()),
                              std::system_category(), "Prefill wait failed");

    throw std::runtime_error("Unexpected prefill wait result");
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
  const size_t padding = render_->paddingFrames();
  if (padding == 0 && fifo_->readableFrames() == 0)
  {
    ++stats_.underruns;
    return false;
  }

  const size_t available_frames = render_->bufferFrames() - padding;
  if (available_frames == 0)
    return false;

  auto view = fifo_->acquireRead();
  if (!view)
    return false;

  const size_t frames = std::min(view.frames(), available_frames);

  if (process_)
    process_(view, frames);
  guardOutput(view, frames);
  render_->write(view, frames);

  view.consume(frames);
  stats_.rendered_frames += frames;
  stats_.observeFifo(*fifo_);

  return true;
}

void AudioEngine::runLoop()
{
  const HANDLE events[] = {control_event_.get(), capture_->eventHandle(), render_->eventHandle()};

  while (!stop_requested_.load(std::memory_order_acquire))
  {
    const DWORD result = ::WaitForMultipleObjects(3, events, FALSE, INFINITE);

    if (result == WAIT_OBJECT_0)
      return;

    if (result == WAIT_OBJECT_0 + 1)
    {
      while (!stop_requested_.load(std::memory_order_acquire) &&
             capture_->nextPacketFrames() > 0)
      {
        capturePacket();
      }

      continue;
    }

    if (result == WAIT_OBJECT_0 + 2)
    {
      while (!stop_requested_.load(std::memory_order_acquire) && renderFromFifo()) {}
      continue;
    }

    if (result == WAIT_FAILED)
      throw std::system_error(static_cast<int>(::GetLastError()),
                              std::system_category(), "Audio event wait failed");

    throw std::runtime_error("Unexpected audio event wait result");
  }
}

void AudioEngine::run()
{
  if (stop_requested_.load(std::memory_order_acquire))
    return;

  fifo_->reset();

  stats_ = {};
  stats_.observeFifo(*fifo_);

  capture_->start();

  try
  {
    prefill();

    if (!stop_requested_.load(std::memory_order_acquire))
    {
      // Prime the render buffer before its clock starts.
      while (!stop_requested_.load(std::memory_order_acquire) &&
             renderFromFifo())
      {}

      if (!stop_requested_.load(std::memory_order_acquire))
      {
        render_->start();
        runLoop();
      }
    }
  }
  catch (...)
  {
    render_->stop();
    capture_->stop();
    throw;
  }

  render_->stop();
  capture_->stop();
}

void AudioEngine::requestStop() noexcept
{
  stop_requested_.store(true, std::memory_order_release);
  ::SetEvent(control_event_.get());
}

} // namespace mitmegy
