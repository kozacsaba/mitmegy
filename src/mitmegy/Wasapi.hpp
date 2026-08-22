#pragma once

#include "AudioFifo.hpp"

#include <windows.h>
#include <audioclient.h>
#include <wrl/client.h>

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace mitmegy
{

struct Endpoint
{
  std::wstring name;
  std::wstring id;
};

enum class SampleFormat
{
  IntegerPcm,
  Float32
};

struct CaptureResult
{
  size_t captured_frames = 0;
  size_t written_frames = 0;
  bool discontinuity = false;
};

struct CoTaskMemDeleter
{
  void operator()(void* ptr) const;
};

using WaveFormatPtr = std::unique_ptr<WAVEFORMATEX, CoTaskMemDeleter>;
using CoTaskMemWString = std::unique_ptr<wchar_t, CoTaskMemDeleter>;

// COM must be initialized on the calling thread.
void enumerateRenderEndpoints(std::vector<Endpoint>& endpoints);

class WasapiCapture
{
public:
  explicit WasapiCapture(const Endpoint& endpoint);
  ~WasapiCapture();

  WasapiCapture(const WasapiCapture&) = delete;
  WasapiCapture& operator=(const WasapiCapture&) = delete;

  const WAVEFORMATEX& format() const;
  SampleFormat sampleFormat() const;

  size_t channels() const;
  size_t sampleRate() const;

  void start();
  void stop() noexcept;

  size_t nextPacketFrames() const;
  CaptureResult captureInto(AudioFifo& fifo);

private:
  size_t writeSilence(AudioFifo& fifo, size_t frames);

  Microsoft::WRL::ComPtr<IAudioClient> audio_client_;
  Microsoft::WRL::ComPtr<IAudioCaptureClient> capture_client_;
  WaveFormatPtr format_;

  bool started_ = false;
};

class WasapiRender
{
public:
  WasapiRender(const Endpoint& endpoint, const WAVEFORMATEX& format);
  ~WasapiRender();

  WasapiRender(const WasapiRender&) = delete;
  WasapiRender& operator=(const WasapiRender&) = delete;

  void start();
  void stop() noexcept;

  size_t bufferFrames() const;
  size_t paddingFrames() const;

  void write(const AudioFifo::ReadView& view, size_t frames);

private:
  Microsoft::WRL::ComPtr<IAudioClient> audio_client_;
  Microsoft::WRL::ComPtr<IAudioRenderClient> render_client_;

  UINT32 buffer_frames_ = 0;
  size_t channels_ = 0;

  bool started_ = false;
};

} // namespace mitmegy
