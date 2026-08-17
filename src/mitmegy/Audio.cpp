#include "Audio.hpp"

#include <windows.h>
#include <audioclient.h>
#include <propkeydef.h>
#include <functiondiscoverykeys_devpkey.h>
#include <mmdeviceapi.h>
#include <wrl/client.h>
#include <ksmedia.h>

#include <algorithm>
#include <cwctype>
#include <cstring>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>

using Microsoft::WRL::ComPtr;

namespace
{

#define MITMEGY_WIDEN_IMPL(value) L##value
#define MITMEGY_WIDEN(value) MITMEGY_WIDEN_IMPL(value)

constexpr std::wstring_view kAudioSinkName =
  MITMEGY_WIDEN(MITMEGY_AUDIO_SINK_NAME);

std::string formatHresult(const char* operation, HRESULT hr)
{
  std::stringstream ss;
  ss << operation << " failed: 0x"
     << std::hex << static_cast<unsigned long>(hr);

  return ss.str(); 
}

bool containsInsensitive(std::wstring_view text, std::wstring_view substring)
{
  if (substring.empty()) return true;

  return std::search(
    text.begin(),
    text.end(),
    substring.begin(),
    substring.end(),
    [](wchar_t lhs, wchar_t rhs)
    {
      return std::towlower(lhs) ==
             std::towlower(rhs);
    }) != text.end();
}

HRESULT createEnumerator(ComPtr<IMMDeviceEnumerator>& enumerator)
{
  return CoCreateInstance(
    __uuidof(MMDeviceEnumerator),
    nullptr,
    CLSCTX_ALL,
    IID_PPV_ARGS(enumerator.GetAddressOf()));
}

HRESULT getDevice(IMMDeviceEnumerator* enumerator,
                  const mitmegy::Endpoint& endpoint,
                  ComPtr<IMMDevice>& device)
{
  return enumerator->GetDevice(
    endpoint.id.c_str(),
    device.GetAddressOf());
}

struct CoTaskMemDeleter
{
  void operator()(void* ptr) const
  {
    CoTaskMemFree(ptr);
  }
};

using WaveFormatPtr = std::unique_ptr<WAVEFORMATEX, CoTaskMemDeleter>;

using CoTaskMemWString = std::unique_ptr<wchar_t, CoTaskMemDeleter>;

struct CaptureSide
{
  ComPtr<IAudioClient> audio_client;
  ComPtr<IAudioCaptureClient> capture_client;
  WaveFormatPtr format;
};

struct RenderSide
{
  ComPtr<IAudioClient> audio_client;
  ComPtr<IAudioRenderClient> render_client;
  UINT32 buffer_frames = 0;
};

CaptureSide createCaptureSide(IMMDevice* device)
{
  CaptureSide capture;

  HRESULT hr = device->Activate(
    __uuidof(IAudioClient),
    CLSCTX_ALL,
    nullptr,
    reinterpret_cast<void**>(capture.audio_client.GetAddressOf()));

  if (FAILED(hr)) throw std::runtime_error(formatHresult("AudioSink Activate", hr));

  WAVEFORMATEX* raw_format = nullptr;
  hr = capture.audio_client->GetMixFormat(&raw_format);
  if (FAILED(hr)) throw std::runtime_error(formatHresult("AudioSink GetMixFormat", hr));

  capture.format.reset(raw_format);

  // SMOKE TEST:
  // Large buffer for simplicity.
  //
  // TODO:
  // Replace with low-latency, event-driven buffering.
  constexpr REFERENCE_TIME buffer_duration = 100 * 10'000;

  hr = capture.audio_client->Initialize(
    AUDCLNT_SHAREMODE_SHARED,
    AUDCLNT_STREAMFLAGS_LOOPBACK,
    buffer_duration,
    0,
    capture.format.get(),
    nullptr);

  if (FAILED(hr)) throw std::runtime_error(formatHresult("Loopback Initialize", hr));

  hr = capture.audio_client->GetService(
    __uuidof(IAudioCaptureClient),
    reinterpret_cast<void**>(capture.capture_client.GetAddressOf()));

  if (FAILED(hr)) throw std::runtime_error(formatHresult("Get IAudioCaptureClient", hr));

  return capture;
}

enum class SampleFormat
{
  IntegerPcm,
  Float32
};

const WAVEFORMATEXTENSIBLE& getExtensibleFormat(const WAVEFORMATEX& format)
{
  if (format.wFormatTag != WAVE_FORMAT_EXTENSIBLE)
    throw std::runtime_error("Audio format is not WAVE_FORMAT_EXTENSIBLE");

  constexpr WORD required_extra_size =
    sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);

  if (format.cbSize < required_extra_size)
    throw std::runtime_error("Invalid WAVEFORMATEXTENSIBLE size");

  return *reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(&format);
}

SampleFormat getSampleFormat(const WAVEFORMATEX& format)
{
  if (format.wFormatTag == WAVE_FORMAT_PCM)
    return SampleFormat::IntegerPcm;

  if (format.wFormatTag == WAVE_FORMAT_IEEE_FLOAT)
  {
    if (format.wBitsPerSample != 32)
      throw std::runtime_error("Unsupported IEEE float sample size");

    return SampleFormat::Float32;
  }

  if (format.wFormatTag != WAVE_FORMAT_EXTENSIBLE)
    throw std::runtime_error("Unsupported audio format");

  const auto& extensible = getExtensibleFormat(format);

  if (IsEqualGUID(extensible.SubFormat, KSDATAFORMAT_SUBTYPE_PCM))
    return SampleFormat::IntegerPcm;

  if (IsEqualGUID(extensible.SubFormat, KSDATAFORMAT_SUBTYPE_IEEE_FLOAT))
  {
    if (format.wBitsPerSample != 32)
      throw std::runtime_error("Unsupported IEEE float sample size");

    return SampleFormat::Float32;
  }

  throw std::runtime_error("Unsupported WAVEFORMATEXTENSIBLE subformat");
}

const wchar_t* sampleFormatName(SampleFormat format)
{
  switch (format)
  {
    case SampleFormat::IntegerPcm:
      return L"Integer PCM";

    case SampleFormat::Float32:
      return L"32-bit IEEE float";
  }

  return L"Unknown";
}

RenderSide createRenderSide(IMMDevice* device, const WAVEFORMATEX* format)
{
  RenderSide render;

  HRESULT hr = device->Activate(
    __uuidof(IAudioClient),
    CLSCTX_ALL,
    nullptr,
    reinterpret_cast<void**>(render.audio_client.GetAddressOf()));
  if (FAILED(hr)) throw std::runtime_error(formatHresult("Output Activate", hr));

  // SMOKE TEST:
  // Let Windows convert the loopback format to the output mix format.
  //
  // TODO:
  // Introduce an explicit internal processing format and conversion.
  constexpr DWORD flags =
    AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM |
    AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY;

  constexpr REFERENCE_TIME buffer_duration = 100 * 10'000;

  hr = render.audio_client->Initialize(
    AUDCLNT_SHAREMODE_SHARED,
    flags,
    buffer_duration,
    0,
    format,
    nullptr);
  if (FAILED(hr)) throw std::runtime_error(formatHresult("Output Initialize", hr));

  hr = render.audio_client->GetBufferSize(&render.buffer_frames);
  if (FAILED(hr)) throw std::runtime_error(formatHresult("Output GetBufferSize", hr));

  hr = render.audio_client->GetService(
    __uuidof(IAudioRenderClient),
    reinterpret_cast<void**>(render.render_client.GetAddressOf()));
  if (FAILED(hr)) throw std::runtime_error(formatHresult("Get IAudioRenderClient", hr));

  return render;
}

void printFormat(const WAVEFORMATEX& format)
{
  const auto sample_format = getSampleFormat(format);

  std::wcout
    << L"Input format:\n"
    << L"  Sample rate: " << format.nSamplesPerSec << L" Hz\n"
    << L"  Channels: " << format.nChannels << L'\n'
    << L"  Bits/sample: " << format.wBitsPerSample << L'\n'
    << L"  Block align: " << format.nBlockAlign << L" bytes/frame\n"
    << L"  Format tag: 0x" << std::hex << format.wFormatTag << std::dec << L'\n'
    << L"  Extra size: " << format.cbSize << L" bytes\n"
    << L"  Sample format: " << sampleFormatName(sample_format) << L'\n';

  if (format.wFormatTag == WAVE_FORMAT_EXTENSIBLE)
  {
    const auto& extensible = getExtensibleFormat(format);

    std::wcout
      << L"  Valid bits/sample: "
      << extensible.Samples.wValidBitsPerSample << L'\n'
      << L"  Channel mask: 0x"
      << std::hex << extensible.dwChannelMask << std::dec << L'\n';
  }

  std::wcout << L'\n';
}

void forwardPacket(CaptureSide& capture, RenderSide& render, uint64_t& dropped_packets)
{
  BYTE* capture_data = nullptr;
  UINT32 frames = 0;
  DWORD capture_flags = 0;

  HRESULT hr = capture.capture_client->GetBuffer(
    &capture_data,
    &frames,
    &capture_flags,
    nullptr,
    nullptr);
  if (FAILED(hr)) throw std::runtime_error(formatHresult("Capture GetBuffer", hr));

  UINT32 padding = 0;
  hr = render.audio_client->GetCurrentPadding(&padding);
  if (FAILED(hr))
  {
    capture.capture_client->ReleaseBuffer(frames);
    throw std::runtime_error(formatHresult("Output GetCurrentPadding", hr));
  }

  const UINT32 available_frames = render.buffer_frames - padding;

  if (frames > available_frames)
  {
    // SMOKE TEST:
    // Drop the complete packet if the output cannot currently accept it.
    //
    // TODO:
    // Replace this with a ring buffer.
    ++dropped_packets;
    capture.capture_client->ReleaseBuffer(frames);
    return;
  }

  BYTE* render_data = nullptr;
  hr = render.render_client->GetBuffer(frames, &render_data);

  if (FAILED(hr))
  {
    capture.capture_client->ReleaseBuffer(frames);
    throw std::runtime_error(formatHresult("Output GetBuffer", hr));
  }

  DWORD output_flags = 0;

  if (capture_flags & AUDCLNT_BUFFERFLAGS_SILENT)
    output_flags = AUDCLNT_BUFFERFLAGS_SILENT;
  else
    std::memcpy(render_data, capture_data, 
      static_cast<size_t>(frames) * capture.format->nBlockAlign);

  hr = render.render_client->ReleaseBuffer(frames, output_flags);

  if (FAILED(hr))
  {
    capture.capture_client->ReleaseBuffer(frames);
    throw std::runtime_error(formatHresult("Output ReleaseBuffer", hr));
  }

  hr = capture.capture_client->ReleaseBuffer(frames);
  if (FAILED(hr)) throw std::runtime_error(formatHresult("Capture ReleaseBuffer", hr));

  return;
}

void runBridgeLoop(CaptureSide& capture, RenderSide& render, 
                   const std::atomic_bool& running, uint64_t& dropped_packets)
{
  while (running.load(std::memory_order_relaxed))
  {
    UINT32 packet_frames = 0;
    HRESULT hr = capture.capture_client->GetNextPacketSize(&packet_frames);
    if (FAILED(hr)) throw std::runtime_error(formatHresult("GetNextPacketSize", hr));

    if (packet_frames == 0)
    {
      // SMOKE TEST:
      // Polling is sufficient for now.
      //
      // TODO:
      // Replace with event-driven WASAPI notification.
      Sleep(1);
      continue;
    }

    while (packet_frames > 0)
    {
      forwardPacket(capture, render, dropped_packets);
      hr = capture.capture_client->GetNextPacketSize(&packet_frames);
      if (FAILED(hr)) throw std::runtime_error(formatHresult("GetNextPacketSize", hr));
    }
  }
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

  const PROPVARIANT& operator*() const
  {
    return value_;
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

void enumerateRenderEndpoints(std::vector<Endpoint>& endpoints)
{
  ComPtr<IMMDeviceEnumerator> enumerator;
  HRESULT hr = createEnumerator(enumerator);
  if (FAILED(hr)) throw std::runtime_error(formatHresult("CoCreateInstance", hr));

  ComPtr<IMMDeviceCollection> collection;

  hr = enumerator->EnumAudioEndpoints(
    eRender,
    DEVICE_STATE_ACTIVE,
    collection.GetAddressOf());
  if (FAILED(hr)) throw std::runtime_error(formatHresult("EnumAudioEndpoints", hr));

  UINT count = 0;
  hr = collection->GetCount(&count);
  if (FAILED(hr)) throw std::runtime_error(formatHresult("GetCount", hr));

  endpoints.clear();
  endpoints.reserve(count);

  for (UINT i = 0; i < count; ++i)
  {
    ComPtr<IMMDevice> device;
    if (FAILED(collection->Item(i, device.GetAddressOf()))) continue;

    ComPtr<IPropertyStore> properties;
    if (FAILED(device->OpenPropertyStore(STGM_READ, properties.GetAddressOf()))) continue;

    PropVariant name;
    hr = properties->GetValue(PKEY_Device_FriendlyName, name.get());
    if (FAILED(hr) || name->vt != VT_LPWSTR) continue;

    LPWSTR raw_id = nullptr;
    hr = device->GetId(&raw_id);
    if (FAILED(hr)) continue;

    CoTaskMemWString id(raw_id);
    endpoints.push_back({name->pwszVal, id.get()});
  }

  return;
}

bool isAudioSink(const Endpoint& endpoint)
{
  return containsInsensitive(
    endpoint.name,
    kAudioSinkName);
}

const Endpoint* findAudioSink(const std::vector<Endpoint>& endpoints)
{
  const auto it = std::find_if(
    endpoints.begin(),
    endpoints.end(),
    isAudioSink);

  if (it == endpoints.end())
    return nullptr;

  return &*it;
}

const Endpoint* findOutputEndpoint(const std::vector<Endpoint>& endpoints, std::wstring_view filter)
{
  const auto it = std::find_if(
    endpoints.begin(),
    endpoints.end(),
    [&](const Endpoint& endpoint)
    {
      return
        !isAudioSink(endpoint) &&
        containsInsensitive(
          endpoint.name,
          filter);
    });

  if (it == endpoints.end())
    return nullptr;

  return &*it;
}

void runAudioBridge(const Endpoint& audio_sink, const Endpoint& output, const std::atomic_bool& running)
{
  ComPtr<IMMDeviceEnumerator> enumerator;
  HRESULT hr = createEnumerator(enumerator);
  if (FAILED(hr)) throw std::runtime_error(formatHresult("CoCreateInstance", hr));

  ComPtr<IMMDevice> input_device;
  hr = getDevice(enumerator.Get(), audio_sink, input_device);
  if (FAILED(hr)) throw std::runtime_error(formatHresult("Get AudioSink device", hr));

  ComPtr<IMMDevice> output_device;
  hr = getDevice(enumerator.Get(), output, output_device);
  if (FAILED(hr)) throw std::runtime_error(formatHresult("Get output device", hr));

  auto capture = createCaptureSide(input_device.Get());
  printFormat(*capture.format);

  auto render = createRenderSide(output_device.Get(), capture.format.get());
  hr = render.audio_client->Start();
  if (FAILED(hr)) throw std::runtime_error(formatHresult("Output Start", hr));

  hr = capture.audio_client->Start();
  if (FAILED(hr))
  {
    render.audio_client->Stop();
    throw std::runtime_error(formatHresult("Loopback Start", hr));
  }

  uint64_t dropped_packets = 0;

  try
  {
    runBridgeLoop(capture, render, running, dropped_packets);
  }
  catch(...)
  {
    capture.audio_client->Stop();
    render.audio_client->Stop();
    throw;
  }

  capture.audio_client->Stop();
  render.audio_client->Stop();

  std::wcout
    << L"\nStopped.\n"
    << L"Dropped packets: " << dropped_packets << L'\n';
}

} // namespace mitmegy
