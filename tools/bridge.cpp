#include <mitmegy/Audio.hpp>

#include <windows.h>

#include <atomic>
#include <iostream>
#include <vector>

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
  HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);

  if (FAILED(hr))
  {
    std::cerr
      << "CoInitializeEx failed: 0x"
      << std::hex
      << static_cast<unsigned long>(hr)
      << '\n';

    return 1;
  }

  SetConsoleCtrlHandler(consoleHandler, TRUE);

  std::vector<mitmegy::Endpoint> endpoints;

  if (!mitmegy::enumerateRenderEndpoints(endpoints))
  {
    CoUninitialize();
    return 1;
  }

  const auto* audio_sink = mitmegy::findAudioSink(endpoints);

  if (audio_sink == nullptr)
  {
    std::wcerr << L"ERROR: AudioSink endpoint " << L"is not available.\n";
    CoUninitialize();
    return 1;
  }

  if (argc < 2)
  {
    printOutputEndpoints(endpoints);
    CoUninitialize();
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
    CoUninitialize();
    return 1;
  }

  std::wcout
    << L"Selected output endpoint:\n"
    << L"    "
    << output->name
    << L'\n'
    << L"    ID: "
    << output->id
    << L"\n\n"
    << L"Forwarding:\n"
    << L"    "
    << audio_sink->name
    << L"\n"
    << L"        -> "
    << output->name
    << L"\n\n"
    << L"Press Ctrl+C to stop.\n";

  const int result = mitmegy::runAudioBridge(
      *audio_sink, *output, g_running);

  CoUninitialize();

  return result;
}
