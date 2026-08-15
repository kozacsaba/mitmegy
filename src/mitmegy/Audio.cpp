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

  ComPtr<IAudioClient> capture_audio_client;

  hr = input_device->Activate(
    __uuidof(IAudioClient),
    CLSCTX_ALL,
    nullptr,
    reinterpret_cast<void**>(capture_audio_client.GetAddressOf()));

  if (FAILED(hr))
  {
    printHresult("AudioSink Activate", hr);
    return 1;
  }

  WAVEFORMATEX* raw_format = nullptr;
  hr = capture_audio_client->GetMixFormat(&raw_format);

  if (FAILED(hr))
  {
    printHresult("AudioSink GetMixFormat", hr);
    return 1;
  }

  std::unique_ptr<WAVEFORMATEX, decltype(&CoTaskMemFree)>
    format(raw_format, &CoTaskMemFree);

  std::wcout
    << L"Input format:\n"
    << L"    Sample rate: "
    << format->nSamplesPerSec
    << L" Hz\n"
    << L"    Channels: "
    << format->nChannels
    << L"\n"
    << L"    Bits/sample: "
    << format->wBitsPerSample
    << L"\n\n";

  // SMOKE TEST:
  // Large buffer for simplicity.
  //
  // TODO:
  // Replace with proper low-latency,
  // event-driven buffering.
  constexpr REFERENCE_TIME buffer_duration = 100 * 10'000;

  hr = capture_audio_client->Initialize(
    AUDCLNT_SHAREMODE_SHARED,
    AUDCLNT_STREAMFLAGS_LOOPBACK,
    buffer_duration,
    0,
    format.get(),
    nullptr);

  if (FAILED(hr))
  {
    printHresult("Loopback Initialize", hr);
    return 1;
  }

  ComPtr<IAudioCaptureClient> capture_client;

  hr = capture_audio_client->GetService(
    __uuidof(IAudioCaptureClient),
    reinterpret_cast<void**>(capture_client.GetAddressOf()));

  if (FAILED(hr))
  {
    printHresult("Get IAudioCaptureClient", hr);
    return 1;
  }

  ComPtr<IAudioClient> render_audio_client;

  hr = output_device->Activate(
    __uuidof(IAudioClient),
    CLSCTX_ALL,
    nullptr,
    reinterpret_cast<void**>(render_audio_client.GetAddressOf()));

  if (FAILED(hr))
  {
    printHresult("Output Activate", hr);
    return 1;
  }

  // SMOKE TEST:
  // Feed the loopback format directly to the
  // render client and let Windows convert it.
  //
  // TODO:
  // Introduce our own internal processing
  // format and explicit conversion.
  constexpr DWORD render_flags =
    AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM |
    AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY;

  hr = render_audio_client->Initialize(
    AUDCLNT_SHAREMODE_SHARED,
    render_flags,
    buffer_duration,
    0,
    format.get(),
    nullptr);

  if (FAILED(hr))
  {
    printHresult("Output Initialize", hr);
    return 1;
  }

  UINT32 render_buffer_frames = 0;
  hr = render_audio_client->GetBufferSize(&render_buffer_frames);

  if (FAILED(hr))
  {
    printHresult("Output GetBufferSize", hr);
    return 1;
  }

  ComPtr<IAudioRenderClient> render_client;

  hr = render_audio_client->GetService(
    __uuidof(IAudioRenderClient),
    reinterpret_cast<void**>(render_client.GetAddressOf()));

  if (FAILED(hr))
  {
    printHresult("Get IAudioRenderClient", hr);
    return 1;
  }

  hr = render_audio_client->Start();

  if (FAILED(hr))
  {
    printHresult("Output Start", hr);
    return 1;
  }

  hr = capture_audio_client->Start();

  if (FAILED(hr))
  {
    printHresult("Loopback Start", hr);
    render_audio_client->Stop();
    return 1;
  }

  uint64_t dropped_packets = 0;
  bool failed = false;

  while (running.load(std::memory_order_relaxed))
  {
    UINT32 packet_frames = 0;
    hr = capture_client->GetNextPacketSize(&packet_frames);

    if (FAILED(hr))
    {
      printHresult("GetNextPacketSize", hr);
      failed = true;
      break;
    }

    if (packet_frames == 0)
    {
      // SMOKE TEST:
      // Polling is good enough for now.
      //
      // TODO:
      // Replace with WASAPI events.
      Sleep(1);
      continue;
    }

    while (packet_frames > 0)
    {
      BYTE* capture_data = nullptr;
      UINT32 frames = 0;
      DWORD capture_flags = 0;

      hr = capture_client->GetBuffer(
        &capture_data,
        &frames,
        &capture_flags,
        nullptr,
        nullptr);

      if (FAILED(hr))
      {
        printHresult("Capture GetBuffer", hr);
        failed = true;
        break;
      }

      UINT32 padding = 0;
      hr = render_audio_client->GetCurrentPadding(&padding);

      if (FAILED(hr))
      {
        printHresult("Output GetCurrentPadding", hr);
        capture_client->ReleaseBuffer(frames);
        failed = true;
        break;
      }

      const UINT32 available_frames = render_buffer_frames - padding;

      if (frames <= available_frames)
      {
        BYTE* render_data = nullptr;
        hr = render_client->GetBuffer(frames, &render_data);

        if (FAILED(hr))
        {
          printHresult("Output GetBuffer", hr);
          capture_client->ReleaseBuffer(frames);
          failed = true;
          break;
        }

        DWORD output_flags = 0;

        if (capture_flags & AUDCLNT_BUFFERFLAGS_SILENT)
          output_flags = AUDCLNT_BUFFERFLAGS_SILENT;
        else
          std::memcpy(render_data, capture_data,
            static_cast<size_t>(frames) * format->nBlockAlign);

        hr = render_client->ReleaseBuffer(frames, output_flags);

        if (FAILED(hr))
        {
          printHresult("Output ReleaseBuffer", hr);
          capture_client->ReleaseBuffer(frames);
          failed = true;
          break;
        }
      }
      else
      {
        // SMOKE TEST:
        // Drop packets when output cannot
        // currently accept them.
        //
        // TODO:
        // Replace with a proper ring buffer.
        ++dropped_packets;
      }

      hr = capture_client->ReleaseBuffer(frames);

      if (FAILED(hr))
      {
        printHresult("Capture ReleaseBuffer", hr);
        failed = true;
        break;
      }

      hr = capture_client->GetNextPacketSize(&packet_frames);

      if (FAILED(hr))
      {
        printHresult("GetNextPacketSize", hr);
        failed = true;
        break;
      }
    }

    if (failed) break;
  }

  capture_audio_client->Stop();
  render_audio_client->Stop();

  std::wcout
    << L"\nStopped.\n"
    << L"Dropped packets: "
    << dropped_packets
    << L'\n';

  return failed ? 1 : 0;
}

} // namespace mitmegy
