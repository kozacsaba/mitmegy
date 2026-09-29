#include "mitmegy/AudioEngine.hpp"
#include "mitmegy/Wasapi.hpp"
#include "mitmegy/Com.hpp"

#include <windows.h>

#include <atomic>
#include <iostream>
#include <vector>
#include <stdexcept>
#include <format>
#include <algorithm>
#include <cwctype>
#include <string_view>

namespace
{

std::atomic_bool g_running{true};

BOOL WINAPI consoleHandler(DWORD event)
{
  if (event == CTRL_C_EVENT)
  {
    g_running.store(false, std::memory_order_relaxed);
    return TRUE;
  }

  return FALSE;
}

#define MITMEGY_WIDEN_IMPL(value) L##value
#define MITMEGY_WIDEN(value) MITMEGY_WIDEN_IMPL(value)

constexpr std::wstring_view kAudioSinkName =
  MITMEGY_WIDEN(MITMEGY_AUDIO_SINK_NAME);

bool containsInsensitive(std::wstring_view text, std::wstring_view substring)
{
  if (substring.empty())
    return true;

  return std::search(
    text.begin(), text.end(),
    substring.begin(), substring.end(),
    [](wchar_t lhs, wchar_t rhs)
    {
      return std::towlower(lhs) == std::towlower(rhs);
    }) != text.end();
}

bool isAudioSink(const mitmegy::Endpoint& endpoint)
{
  return containsInsensitive(endpoint.name, kAudioSinkName);
}

const mitmegy::Endpoint* findAudioSink(const std::vector<mitmegy::Endpoint>& endpoints)
{
  const auto it = std::find_if(
    endpoints.begin(), endpoints.end(),
    isAudioSink);

  return it == endpoints.end()
    ? nullptr
    : &*it;
}

const mitmegy::Endpoint* findOutputEndpoint(
  const std::vector<mitmegy::Endpoint>& endpoints,
  std::wstring_view filter)
{
  const auto it = std::find_if(
    endpoints.begin(), endpoints.end(),
    [&](const mitmegy::Endpoint& endpoint)
    {
      return 
        !isAudioSink(endpoint) &&
        containsInsensitive(endpoint.name, filter);
    });

  return it == endpoints.end()
    ? nullptr
    : &*it;
}

void printFormat(const WAVEFORMATEX& format)
{
  std::wcout
    << L"Input format:\n"
    << L"  Sample rate: "
    << format.nSamplesPerSec << L" Hz\n"
    << L"  Channels: "
    << format.nChannels << L'\n'
    << L"  Bits/sample: "
    << format.wBitsPerSample << L'\n'
    << L"  Block align: "
    << format.nBlockAlign << L" bytes/frame\n\n";
}

void printStats(const mitmegy::TransportStats& stats)
{
  std::wcout
    << L"\nStopped.\n"
    << L"Captured frames: "
    << stats.captured_frames << L'\n'
    << L"Rendered frames: "
    << stats.rendered_frames << L'\n'
    << L"FIFO fill: "
    << stats.fifo_fill << L'\n'
    << L"FIFO min fill: "
    << stats.fifo_min_fill << L'\n'
    << L"FIFO max fill: "
    << stats.fifo_max_fill << L'\n'
    << L"Underruns: "
    << stats.underruns << L'\n'
    << L"Overruns: "
    << stats.overruns << L'\n'
    << L"Dropped frames: "
    << stats.dropped_frames << L'\n'
    << L"Capture discontinuities: "
    << stats.capture_discontinuities << L'\n';
}

void printOutputEndpoints(const std::vector<mitmegy::Endpoint>& endpoints)
{
  std::wcout << L"Available output endpoints:\n\n";
  size_t index = 0;

  for (const auto& endpoint : endpoints)
  {
    if (isAudioSink(endpoint))
      continue;

    std::wcout
      << L"["
      << index++
      << L"] "
      << endpoint.name
      << L'\n'
      << L"    ID: "
      << endpoint.id
      << L'\n';
  }
}

} // namespace

int wmain(int argc, wchar_t* argv[])
{
  try
  {
    mitmegy::ComScope com;

    if (!com.ok()) throw std::runtime_error(
        "CoInitializeEx failed: 0x" + 
        std::format("{:x}", (unsigned long)com.result()));

    SetConsoleCtrlHandler(consoleHandler, TRUE);

    std::vector<mitmegy::Endpoint> endpoints;
    mitmegy::enumerateRenderEndpoints(endpoints);

    const auto* audio_sink = findAudioSink(endpoints);

    if (audio_sink == nullptr)
      throw std::runtime_error("ERROR: AudioSink endpoint is not available.");

    if (argc < 2)
    {
      printOutputEndpoints(endpoints);
      return 0;
    }

    const auto* output = findOutputEndpoint(endpoints, argv[1]);

    if (output == nullptr)
    {
      std::wcerr
        << L"ERROR: No output endpoint "
        << L"matches \""
        << argv[1]
        << L"\".\n\n";

      printOutputEndpoints(endpoints);
      return 1;
    }

    std::wcout
      << L"Selected output endpoint:\n"
      << L"    " << output->name << L'\n'
      << L"    ID: " << output->id << L"\n\n"
      << L"Forwarding:\n"
      << L"    " << audio_sink->name << L"\n"
      << L"        -> " << output->name << L"\n\n";

    auto process = [](mitmegy::AudioFifo::ReadView&, size_t)
    {
      // Passthrough for now.
    };

    mitmegy::AudioEngine engine(*audio_sink, *output);
    engine.process_ = std::move(process);
    printFormat(engine.inputFormat());
    std::wcout << L"Press Ctrl+C to stop.\n";

    engine.run(g_running);
    
    printStats(engine.stats());

    return 0;
  }
  catch(const std::exception& e)
  {
    std::cout << e.what();
    return 1;
  }
}
