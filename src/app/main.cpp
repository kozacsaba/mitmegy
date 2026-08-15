#include <mitmegy/Audio.hpp>

#include <windows.h>

#include <iostream>
#include <vector>

int wmain()
{
  HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);

  if (FAILED(hr))
  {
    std::cerr << "Failed to initialize COM.\n";
    return 1;
  }

  std::vector<mitmegy::Endpoint> endpoints;

  if (!mitmegy::enumerateRenderEndpoints(endpoints))
  {
    CoUninitialize();
    return 1;
  }

  const auto* audio_sink = mitmegy::findAudioSink(endpoints);

  if (audio_sink == nullptr)
  {
    std::wcerr << L"AudioSink is not available.\n";
    CoUninitialize();
    return 1;
  }

  std::wcout
    << L"AudioSink available:\n"
    << L"    "
    << audio_sink->name
    << L'\n';

  CoUninitialize();

  return 0;
}
