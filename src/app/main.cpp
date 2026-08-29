#include "mitmegy/AudioEngine.hpp"
#include "mitmegy/Wasapi.hpp"
#include "mitmegy/Com.hpp"
#include "vsthost/PluginChain.hpp"
#include "vsthost/PluginRegistry.hpp"

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
    // MARK: PREP
    // =========================================================================

    mitmegy::ComScope com;

    if (!com.ok()) throw std::runtime_error(
        "CoInitializeEx failed: 0x" + 
        std::format("{:x}", (unsigned long)com.result()));

    SetConsoleCtrlHandler(consoleHandler, TRUE);

    // MARK: ENDPOINTS
    // =========================================================================

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

    // MARK: VST SELECT

    mitmegy::PluginRegistry registry;
    std::cout << "Scanning plugins" << std::endl;
    registry.scan();
    std::cout << std::endl;

    const auto& plugins = registry.plugins().getTypes();

    for (int i = 0; i < plugins.size(); ++i)
    {
      const auto& plugin = plugins.getReference(i);

      std::cout
        << "[" << i << "] "
        << plugin.name
        << " - "
        << plugin.manufacturerName
        << '\n';
    }

    std::cout << "\nSelect plugin index: ";

    int index = -1;
    std::cin >> index;

    if (index < 0 || index >= plugins.size())
      throw std::runtime_error("Invalid plugin index");

    const auto& description = plugins.getReference(index);

    std::cout
      << "\nSelected plugin:\n"
      << "  Name: " << description.name << '\n'
      << "  Manufacturer: " << description.manufacturerName << '\n'
      << "  Format: " << description.pluginFormatName << '\n'
      << "  Identifier: " << description.createIdentifierString() << '\n'
      << "  File: " << description.fileOrIdentifier << "\n\n";

    auto plugin = registry.createInstance(description);

    std::cout
      << "\nCreated instance: "
      << plugin->getName()
      << '\n';

    // MARK: VST HOST

    mitmegy::PluginChain plugin_chain;
    plugin_chain.add(std::move(plugin));
    auto process = [&](mitmegy::AudioFifo::ReadView& view, size_t frames)
    {
      std::array<float*, 8> channels{};

      for (size_t channel = 0; channel < view.channels(); ++channel)
        channels[channel] = view.channel(channel).data();

      juce::AudioBuffer<float> buffer(
        channels.data(),
        static_cast<int>(view.channels()),
        static_cast<int>(frames));

      plugin_chain.processBlock(buffer);
    };

    mitmegy::AudioEngine engine(*audio_sink, *output, process);
    const auto& format = engine.inputFormat();
    plugin_chain.prepareToPlay(format.nSamplesPerSec, engine.maxBlockSize());
    printFormat(format);

    std::wcout << L"Press Ctrl+C to stop.\n";
    engine.run(g_running);

    plugin_chain.releaseResources();
    printStats(engine.stats());

    return 0;
  }
  catch(const std::exception& e)
  {
    std::cout << e.what();
    return 1;
  }
}
