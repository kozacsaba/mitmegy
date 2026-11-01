#include "Wasapi.hpp"

#include <ksmedia.h>
#include <mmdeviceapi.h>
#include <propkeydef.h>
#include <functiondiscoverykeys_devpkey.h>
#include <system_error>

#include <utility>
#include <algorithm>
#include <memory>
#include <sstream>
#include <stdexcept>

using Microsoft::WRL::ComPtr;

namespace
{

mitmegy::WasapiError wasapiError(const char* operation, HRESULT hr)
{
  std::stringstream ss;

  ss << operation << " failed: 0x"
     << std::hex
     << static_cast<unsigned long>(hr);

  return {ss.str(), hr};
}

ComPtr<IMMDeviceEnumerator> createEnumerator()
{
  ComPtr<IMMDeviceEnumerator> enumerator;

  const HRESULT hr = CoCreateInstance(
    __uuidof(MMDeviceEnumerator), nullptr,
    CLSCTX_ALL, IID_PPV_ARGS(enumerator.GetAddressOf()));

  if (FAILED(hr)) throw wasapiError("CoCreateInstance", hr);
  return enumerator;
}

ComPtr<IMMDevice> getDevice(const mitmegy::Endpoint& endpoint)
{
  auto enumerator = createEnumerator();

  ComPtr<IMMDevice> device;
  const HRESULT hr = enumerator->GetDevice(
    endpoint.id.c_str(), device.GetAddressOf());

  if (FAILED(hr)) throw wasapiError("GetDevice", hr);
  return device;
}

const WAVEFORMATEXTENSIBLE& getExtensibleFormat(const WAVEFORMATEX& format)
{
  if (format.wFormatTag != WAVE_FORMAT_EXTENSIBLE)
    throw std::runtime_error("Audio format is not WAVE_FORMAT_EXTENSIBLE");

  constexpr WORD required_extra_size = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);

  if (format.cbSize < required_extra_size)
    throw std::runtime_error("Invalid WAVEFORMATEXTENSIBLE size");

  return *reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(&format);
}

mitmegy::SampleFormat getSampleFormat(const WAVEFORMATEX& format)
{
  if (format.wFormatTag == WAVE_FORMAT_PCM)
    return mitmegy::SampleFormat::IntegerPcm;

  if (format.wFormatTag == WAVE_FORMAT_IEEE_FLOAT)
  {
    if (format.wBitsPerSample != 32)
      throw std::runtime_error("Unsupported IEEE float sample size");

    return mitmegy::SampleFormat::Float32;
  }

  if (format.wFormatTag != WAVE_FORMAT_EXTENSIBLE)
    throw std::runtime_error("Unsupported audio format");

  const auto& extensible = getExtensibleFormat(format);

  if (IsEqualGUID(extensible.SubFormat, KSDATAFORMAT_SUBTYPE_PCM))
    return mitmegy::SampleFormat::IntegerPcm;

  if (IsEqualGUID(extensible.SubFormat, KSDATAFORMAT_SUBTYPE_IEEE_FLOAT))
  {
    if (format.wBitsPerSample != 32)
      throw std::runtime_error("Unsupported IEEE float sample size");

    return mitmegy::SampleFormat::Float32;
  }

  throw std::runtime_error("Unsupported WAVEFORMATEXTENSIBLE subformat");
}

class PropVariant
{
public:
  PropVariant()
  {
    PropVariantInit(&value_);
  }

  ~PropVariant()
  {
    PropVariantClear(&value_);
  }

  PropVariant(const PropVariant&) = delete;
  PropVariant& operator=(const PropVariant&) = delete;

  PROPVARIANT* get()
  {
    return &value_;
  }

  const PROPVARIANT* operator->() const
  {
    return &value_;
  }

private:
  PROPVARIANT value_;
};

} // namespace

namespace mitmegy
{

void CoTaskMemDeleter::operator()(void* ptr) const
{
  CoTaskMemFree(ptr);
}

void enumerateRenderEndpoints(std::vector<Endpoint>& endpoints)
{
  auto enumerator = createEnumerator();

  ComPtr<IMMDeviceCollection> collection;
  HRESULT hr = enumerator->EnumAudioEndpoints(
    eRender, DEVICE_STATE_ACTIVE,
    collection.GetAddressOf());
  if (FAILED(hr))
    throw wasapiError("EnumAudioEndpoints", hr);

  UINT count = 0;
  hr = collection->GetCount(&count);
  if (FAILED(hr))
    throw wasapiError("GetCount", hr);

  endpoints.clear();
  endpoints.reserve(count);

  for (UINT i = 0; i < count; ++i)
  {
    ComPtr<IMMDevice> device;
    if (FAILED(collection->Item(i, device.GetAddressOf())))
      continue;

    ComPtr<IPropertyStore> properties;
    if (FAILED(device->OpenPropertyStore(STGM_READ, properties.GetAddressOf())))
      continue;

    PropVariant name;
    hr = properties->GetValue(PKEY_Device_FriendlyName, name.get());
    if (FAILED(hr) || name->vt != VT_LPWSTR)
      continue;

    LPWSTR raw_id = nullptr;
    hr = device->GetId(&raw_id);
    if (FAILED(hr))
      continue;

    CoTaskMemWString id(raw_id);
    endpoints.push_back({name->pwszVal, id.get() });
  }
}

WasapiError::WasapiError(std::string message, HRESULT hr)
  : std::runtime_error(std::move(message))
  , hr_(hr)
{}

HRESULT WasapiError::hresult() const noexcept
{
  return hr_;
}

UniqueHandle createEvent()
{
  UniqueHandle event(::CreateEventW(nullptr, FALSE, FALSE, nullptr));

  if (!event)
    throw std::system_error(
      static_cast<int>(::GetLastError()),
      std::system_category(),
      "CreateEventW");

  return event;
}

// WasapiCapture

WasapiCapture::WasapiCapture(const Endpoint& endpoint)
  : event_(createEvent())
{
  auto device = getDevice(endpoint);

  HRESULT hr = device->Activate(
    __uuidof(IAudioClient), CLSCTX_ALL, nullptr,
    reinterpret_cast<void**>(audio_client_.GetAddressOf()));

  if (FAILED(hr))
    throw wasapiError("AudioSink Activate", hr);

  WAVEFORMATEX* raw_format = nullptr;
  hr = audio_client_->GetMixFormat(&raw_format);

  if (FAILED(hr))
    throw wasapiError("AudioSink GetMixFormat", hr);

  format_.reset(raw_format);

  constexpr DWORD flags =
    AUDCLNT_STREAMFLAGS_LOOPBACK |
    AUDCLNT_STREAMFLAGS_EVENTCALLBACK;

  hr = audio_client_->Initialize(
    AUDCLNT_SHAREMODE_SHARED, flags,
    0, 0, format_.get(), nullptr);

  if (FAILED(hr))
    throw wasapiError("Loopback Initialize", hr);

  hr = audio_client_->SetEventHandle(event_.get());

  if (FAILED(hr))
    throw wasapiError("Loopback SetEventHandle", hr);

  hr = audio_client_->GetService(
    __uuidof(IAudioCaptureClient),
    reinterpret_cast<void**>(capture_client_.GetAddressOf()));

  if (FAILED(hr))
    throw wasapiError("Get IAudioCaptureClient", hr);
}

WasapiCapture::~WasapiCapture()
{
  stop();
}

const WAVEFORMATEX& WasapiCapture::format() const
{
  return *format_;
}

SampleFormat WasapiCapture::sampleFormat() const
{
  return getSampleFormat(*format_);
}

HANDLE WasapiCapture::eventHandle() const noexcept
{
  return event_.get();
}

size_t WasapiCapture::channels() const
{
  return format_->nChannels;
}

size_t WasapiCapture::sampleRate() const
{
  return format_->nSamplesPerSec;
}

void WasapiCapture::start()
{
  if (started_)
    return;

  if (!::ResetEvent(event_.get()))
    throw std::system_error(
      static_cast<int>(::GetLastError()),
      std::system_category(),
      "ResetEvent(capture)");

  const HRESULT hr = audio_client_->Start();
  if (FAILED(hr))
    throw wasapiError("Loopback Start", hr);

  started_ = true;
}

void WasapiCapture::stop() noexcept
{
  if (!started_)
    return;

  audio_client_->Stop();
  started_ = false;
}

size_t WasapiCapture::nextPacketFrames() const
{
  UINT32 frames = 0;

  const HRESULT hr = capture_client_->GetNextPacketSize(&frames);
  if (FAILED(hr))
    throw wasapiError("GetNextPacketSize", hr);

  return frames;
}

size_t WasapiCapture::writeSilence(AudioFifo& fifo, size_t frames)
{
  size_t written = 0;
  while (written < frames)
  {
    auto view = fifo.acquireWrite();

    if (!view)
      break;

    const size_t chunk = std::min(view.frames(), frames - written);
    for (size_t channel = 0; channel < view.channels(); ++channel)
    {
      auto samples = view.channel(channel);
      std::fill_n(samples.begin(), chunk, 0.0f);
    }

    view.commit(chunk);
    written += chunk;
  }

  return written;
}

CaptureResult WasapiCapture::captureInto(AudioFifo& fifo)
{
  if (fifo.channels() != channels())
    throw std::invalid_argument("FIFO channel count does not match capture stream");

  BYTE* capture_data = nullptr;
  UINT32 frames = 0;
  DWORD flags = 0;

  HRESULT hr = capture_client_->GetBuffer(
    &capture_data, &frames, &flags,
    nullptr, nullptr);

  if (FAILED(hr))
    throw wasapiError("Capture GetBuffer", hr);

  size_t written = 0;
  if (flags & AUDCLNT_BUFFERFLAGS_SILENT)
  {
    written = writeSilence(fifo, frames);
  }
  else
  {
    const auto* samples = reinterpret_cast<const float*>(capture_data);

    written = fifo.writeInterleaved(
      std::span<const float>(samples, static_cast<size_t>(frames) * channels()),
      frames);
  }

  hr = capture_client_->ReleaseBuffer(frames);

  if (FAILED(hr))
    throw wasapiError("Capture ReleaseBuffer", hr);

  return {frames, written, (flags & AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY) != 0};
}

// WasapiRender

WasapiRender::WasapiRender(const Endpoint& endpoint, const WAVEFORMATEX& format)
  : event_(createEvent())
  , channels_(format.nChannels)
{
  auto device = getDevice(endpoint);

  HRESULT hr = device->Activate(
    __uuidof(IAudioClient), CLSCTX_ALL, nullptr,
    reinterpret_cast<void**>(audio_client_.GetAddressOf()));

  if (FAILED(hr))
    throw wasapiError("Output Activate", hr);

  constexpr DWORD flags =
    AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM |
    AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY |
    AUDCLNT_STREAMFLAGS_EVENTCALLBACK;

  hr = audio_client_->Initialize(
    AUDCLNT_SHAREMODE_SHARED, flags,
    0, 0, &format, nullptr);

  if (FAILED(hr))
    throw wasapiError("Output Initialize", hr);

  hr = audio_client_->SetEventHandle(event_.get());

  if (FAILED(hr))
    throw wasapiError("Output SetEventHandle", hr);

  hr = audio_client_->GetBufferSize(&buffer_frames_);

  if (FAILED(hr))
    throw wasapiError("Output GetBufferSize", hr);

  hr = audio_client_->GetService(
    __uuidof(IAudioRenderClient),
    reinterpret_cast<void**>(render_client_.GetAddressOf()));

  if (FAILED(hr))
    throw wasapiError("Get IAudioRenderClient", hr);
}

WasapiRender::~WasapiRender()
{
  stop();
}

HANDLE WasapiRender::eventHandle() const noexcept
{
  return event_.get();
}

void WasapiRender::start()
{
  if (started_)
    return;

  if (!::ResetEvent(event_.get()))
    throw std::system_error(
      static_cast<int>(::GetLastError()),
      std::system_category(),
      "ResetEvent(render)");

  const HRESULT hr = audio_client_->Start();
  if (FAILED(hr))
    throw wasapiError("Output Start", hr);

  started_ = true;
}

void WasapiRender::stop() noexcept
{
  if (!started_)
    return;

  audio_client_->Stop();
  started_ = false;
}

size_t WasapiRender::bufferFrames() const
{
  return buffer_frames_;
}

size_t WasapiRender::paddingFrames() const
{
  UINT32 padding = 0;
  const HRESULT hr = audio_client_->GetCurrentPadding(&padding);

  if (FAILED(hr))
    throw wasapiError("Output GetCurrentPadding", hr);

  return padding;
}

void WasapiRender::write(const AudioFifo::ReadView& view, size_t frames)
{
  if (view.channels() != channels_)
    throw std::invalid_argument("Audio channel count does not match render stream");

  if (frames > view.frames())
    throw std::invalid_argument("Requested render size exceeds audio view");

  BYTE* render_data = nullptr;
  HRESULT hr = render_client_->GetBuffer(static_cast<UINT32>(frames), &render_data);

  if (FAILED(hr))
    throw wasapiError("Output GetBuffer", hr);

  auto* output = reinterpret_cast<float*>(render_data);
  for (size_t frame = 0; frame < frames; ++frame)
    for (size_t channel = 0; channel < channels_; ++channel)
      output[frame * channels_ + channel] = view.channel(channel)[frame];

  hr = render_client_->ReleaseBuffer(static_cast<UINT32>(frames), 0);
  if (FAILED(hr))
    throw wasapiError("Output ReleaseBuffer", hr);
}

} // namespace mitmegy
