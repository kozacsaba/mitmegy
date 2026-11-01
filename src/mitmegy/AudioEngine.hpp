#pragma once

#ifndef NOMINMAX
  #define NOMINMAX
#endif

#include "AudioFifo.hpp"
#include "Wasapi.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>

namespace mitmegy
{

struct TransportStats
{
  uint64_t captured_frames = 0;
  uint64_t rendered_frames = 0;

  uint64_t underruns = 0;
  uint64_t overruns = 0;
  uint64_t dropped_frames = 0;
  uint64_t capture_discontinuities = 0;

  size_t fifo_fill = 0;
  size_t fifo_min_fill = std::numeric_limits<size_t>::max();
  size_t fifo_max_fill = 0;

  void observeFifo(const AudioFifo& fifo);
};

using ProcessCallback = std::function<void(AudioFifo::ReadView&, size_t)>;

class AudioEngine
{
public:
  AudioEngine(const Endpoint& audio_sink, const Endpoint& output);

  void run();
  void requestStop() noexcept;
  const TransportStats& stats() const;

  const WAVEFORMATEX& inputFormat() const;
  SampleFormat inputSampleFormat() const;
  size_t maxBlockSize() const;

  ProcessCallback process_;

private:
  void capturePacket();
  void prefill();
  void runLoop();
  bool renderFromFifo();
  void guardOutput(AudioFifo::ReadView& view, size_t frames);

  Endpoint audio_sink_;
  Endpoint output_;

  UniqueHandle control_event_;
  std::atomic_bool stop_requested_{false};

  std::unique_ptr<WasapiCapture> capture_;
  std::unique_ptr<AudioFifo> fifo_;
  std::unique_ptr<WasapiRender> render_;

  TransportStats stats_;
};

} // namespace mitmegy
