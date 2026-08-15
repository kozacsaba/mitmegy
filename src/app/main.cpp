#include "mitmegy/Audio.hpp"
#include "mitmegy/Com.hpp"

#include <windows.h>

#include <iostream>
#include <vector>
#include <stdexcept>
#include <format>

int wmain()
{
  try
  {
    mitmegy::ComScope com;

    if (!com.ok()) throw std::runtime_error(
        "CoInitializeEx failed: 0x" + 
        std::format("{:x}", (unsigned long)com.result()));

    std::vector<mitmegy::Endpoint> endpoints;
    mitmegy::enumerateRenderEndpoints(endpoints);

    const auto* audio_sink = mitmegy::findAudioSink(endpoints);

    if (audio_sink == nullptr)
      throw std::runtime_error("AudioSink is not available.");

    std::wcout
      << L"AudioSink available:\n"
      << L"    "
      << audio_sink->name
      << L'\n';

    return 0;
  }
  catch(const std::exception& e)
  {
    std::cout << e.what();
    return 1;
  }
}
