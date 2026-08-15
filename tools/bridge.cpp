#include "mitmegy/Audio.hpp"
#include "mitmegy/Com.hpp"

#include <windows.h>

#include <atomic>
#include <iostream>
#include <vector>
#include <stdexcept>
#include <format>

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

void printOutputEndpoints(const std::vector<mitmegy::Endpoint>& endpoints)
{
  std::wcout << L"Available output endpoints:\n\n";
  size_t index = 0;

  for (const auto& endpoint : endpoints)
  {
    if (mitmegy::isAudioSink(endpoint))
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

    const auto* audio_sink = mitmegy::findAudioSink(endpoints);

    if (audio_sink == nullptr)
      throw std::runtime_error("ERROR: AudioSink endpoint is not available.");

    if (argc < 2)
    {
      printOutputEndpoints(endpoints);
      return 0;
    }

    const auto* output = mitmegy::findOutputEndpoint(endpoints, argv[1]);

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
      << L"        -> " << output->name << L"\n\n"
      << L"Press Ctrl+C to stop.\n";

    mitmegy::runAudioBridge(*audio_sink, *output, g_running);

    return 0;
  }
  catch(const std::exception& e)
  {
    std::cout << e.what();
    return 1;
  }
}
