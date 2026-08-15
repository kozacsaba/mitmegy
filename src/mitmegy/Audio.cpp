#include "Audio.hpp"

#include <windows.h>
#include <audioclient.h>
#include <propkeydef.h>
#include <functiondiscoverykeys_devpkey.h>
#include <mmdeviceapi.h>
#include <wrl/client.h>

#include <algorithm>
#include <cwctype>
#include <cstring>
#include <iostream>
#include <memory>
#include <optional>

using Microsoft::WRL::ComPtr;

namespace
{

#define MITMEGY_WIDEN_IMPL(value) L##value
#define MITMEGY_WIDEN(value) MITMEGY_WIDEN_IMPL(value)

constexpr std::wstring_view kAudioSinkName =
  MITMEGY_WIDEN(MITMEGY_AUDIO_SINK_NAME);

void printHresult(const char* operation, HRESULT hr)
{
  std::cerr
    << operation
    << " failed: 0x"
    << std::hex
    << static_cast<unsigned long>(hr)
    << std::dec
    << '\n';
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

std::optional<CaptureSide> createCaptureSide(IMMDevice* device)
{
  CaptureSide capture;

  HRESULT hr = device->Activate(
    __uuidof(IAudioClient),
    CLSCTX_ALL,
    nullptr,
    reinterpret_cast<void**>(capture.audio_client.GetAddressOf()));

  if (FAILED(hr))
  {
    printHresult("AudioSink Activate", hr);
    return std::nullopt;
  }

  WAVEFORMATEX* raw_format = nullptr;
  hr = capture.audio_client->GetMixFormat(&raw_format);

  if (FAILED(hr))
  {
    printHresult("AudioSink GetMixFormat", hr);
    return std::nullopt;
  }

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

  if (FAILED(hr))
  {
    printHresult("Loopback Initialize", hr);
    return std::nullopt;
  }

  hr = capture.audio_client->GetService(
    __uuidof(IAudioCaptureClient),
    reinterpret_cast<void**>(capture.capture_client.GetAddressOf()));

  if (FAILED(hr))
  {
    printHresult("Get IAudioCaptureClient", hr);
    return std::nullopt;
  }

  return capture;
}

std::optional<RenderSide> createRenderSide(IMMDevice* device, const WAVEFORMATEX* format)
{
  RenderSide render;

  HRESULT hr = device->Activate(
    __uuidof(IAudioClient),
    CLSCTX_ALL,
    nullptr,
    reinterpret_cast<void**>(render.audio_client.GetAddressOf()));

  if (FAILED(hr))
  {
    printHresult("Output Activate", hr);
    return std::nullopt;
  }

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

  if (FAILED(hr))
  {
    printHresult("Output Initialize", hr);
    return std::nullopt;
  }

  hr = render.audio_client->GetBufferSize(&render.buffer_frames);

  if (FAILED(hr))
  {
    printHresult("Output GetBufferSize", hr);
    return std::nullopt;
  }

  hr = render.audio_client->GetService(
    __uuidof(IAudioRenderClient),
    reinterpret_cast<void**>(render.render_client.GetAddressOf()));

  if (FAILED(hr))
  {
    printHresult("Get IAudioRenderClient", hr);
    return std::nullopt;
  }

  return render;
}

void printFormat(const WAVEFORMATEX& format)
{
  std::wcout
    << L"Input format:\n"
    << L"  Sample rate: " << format.nSamplesPerSec << L" Hz\n"
    << L"  Channels: " << format.nChannels << L'\n'
    << L"  Bits/sample: " << format.wBitsPerSample << L"\n\n";
}

bool forwardPacket(CaptureSide& capture, RenderSide& render, uint64_t& dropped_packets)
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

  if (FAILED(hr))
  {
    printHresult("Capture GetBuffer", hr);
    return false;
  }

  UINT32 padding = 0;
  hr = render.audio_client->GetCurrentPadding(&padding);

  if (FAILED(hr))
  {
    printHresult("Output GetCurrentPadding", hr);
    capture.capture_client->ReleaseBuffer(frames);
    return false;
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
    return true;
  }

  BYTE* render_data = nullptr;
  hr = render.render_client->GetBuffer(frames, &render_data);

  if (FAILED(hr))
  {
    printHresult("Output GetBuffer", hr);
    capture.capture_client->ReleaseBuffer(frames);
    return false;
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
    printHresult("Output ReleaseBuffer", hr);
    capture.capture_client->ReleaseBuffer(frames);
    return false;
  }

  hr = capture.capture_client->ReleaseBuffer(frames);

  if (FAILED(hr))
  {
    printHresult("Capture ReleaseBuffer", hr);
    return false;
  }

  return true;
}

bool runBridgeLoop(CaptureSide& capture, RenderSide& render, 
                   const std::atomic_bool& running, uint64_t& dropped_packets)
{
  while (running.load(std::memory_order_relaxed))
  {
    UINT32 packet_frames = 0;
    HRESULT hr = capture.capture_client->GetNextPacketSize(&packet_frames);

    if (FAILED(hr))
    {
      printHresult("GetNextPacketSize", hr);
      return false;
    }

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
      if (!forwardPacket(capture, render, dropped_packets))
        return false;

      hr = capture.capture_client->GetNextPacketSize(&packet_frames);

      if (FAILED(hr))
      {
        printHresult("GetNextPacketSize", hr);
        return false;
      }
    }
  }

  return true;
}

} // namespace

namespace mitmegy
{

bool enumerateRenderEndpoints(std::vector<Endpoint>& endpoints)
{
  ComPtr<IMMDeviceEnumerator> enumerator;
  HRESULT hr = createEnumerator(enumerator);

  if (FAILED(hr))
  {
    printHresult("CoCreateInstance", hr);
    return false;
  }

  ComPtr<IMMDeviceCollection> collection;

  hr = enumerator->EnumAudioEndpoints(
    eRender,
    DEVICE_STATE_ACTIVE,
    collection.GetAddressOf());

  if (FAILED(hr))
  {
    printHresult("EnumAudioEndpoints", hr);
    return false;
  }

  UINT count = 0;

  hr = collection->GetCount(&count);

  if (FAILED(hr))
  {
    printHresult("GetCount", hr);
    return false;
  }

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

    PROPVARIANT name;
    PropVariantInit(&name);

    hr = properties->GetValue(PKEY_Device_FriendlyName, &name);

    if (FAILED(hr) || name.vt != VT_LPWSTR)
    {
      PropVariantClear(&name);
      continue;
    }

    LPWSTR id = nullptr;
    hr = device->GetId(&id);

    if (FAILED(hr))
    {
      PropVariantClear(&name);
      continue;
    }

    endpoints.push_back({name.pwszVal, id});

    CoTaskMemFree(id);
    PropVariantClear(&name);
  }

  return true;
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

int runAudioBridge(const Endpoint& audio_sink, const Endpoint& output, const std::atomic_bool& running)
{
  ComPtr<IMMDeviceEnumerator> enumerator;
  HRESULT hr = createEnumerator(enumerator);

  if (FAILED(hr))
  {
    printHresult("CoCreateInstance", hr);
    return 1;
  }

  ComPtr<IMMDevice> input_device;
  hr = getDevice(enumerator.Get(), audio_sink, input_device);

  if (FAILED(hr))
  {
    printHresult("Get AudioSink device", hr);
    return 1;
  }

  ComPtr<IMMDevice> output_device;
  hr = getDevice(enumerator.Get(), output, output_device);

  if (FAILED(hr))
  {
    printHresult("Get output device", hr);
    return 1;
  }

  auto capture = createCaptureSide(input_device.Get());
  if (!capture) return 1;

  printFormat(*capture->format);

  auto render = createRenderSide(output_device.Get(), capture->format.get());
  if (!render) return 1;

  hr = render->audio_client->Start();

  if (FAILED(hr))
  {
    printHresult("Output Start", hr);
    return 1;
  }

  hr = capture->audio_client->Start();

  if (FAILED(hr))
  {
    printHresult("Loopback Start", hr);
    render->audio_client->Stop();
    return 1;
  }

  uint64_t dropped_packets = 0;

  const bool success = runBridgeLoop(*capture, *render, running, dropped_packets);

  capture->audio_client->Stop();
  render->audio_client->Stop();

  std::wcout
    << L"\nStopped.\n"
    << L"Dropped packets: " << dropped_packets << L'\n';

  return success ? 0 : 1;
}

} // namespace mitmegy
