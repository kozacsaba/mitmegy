#include "Engine.hpp"
#include "MessageHandler.hpp"

#include "vsthost/PluginChain.hpp"
#include "vsthost/PluginRegistry.hpp"

#include <windows.h>

#include <atomic>
#include <iostream>
#include <vector>
#include <stdexcept>
#include <algorithm>
#include <string_view>
#include <thread>
#include <string>
#include <optional>

namespace
{

std::atomic_bool g_running{true};

BOOL WINAPI consoleHandler(DWORD event)
{
  if (event == CTRL_C_EVENT)
  {
    g_running.store(false, std::memory_order_relaxed);
    g_running.notify_all();
    return TRUE;
  }

  return FALSE;
}

} // namespace

int wmain(int argc, wchar_t* argv[])
{
  try
  {
    SetConsoleCtrlHandler(consoleHandler, TRUE);

    mitmegy::Engine engine{};
    mitmegy::PluginRegistry registry;
    mitmegy::PluginChain plugin_chain;

    // restore last session / snapshot
    
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

    engine.setProcessCallback(std::move(process));

    const auto format = engine.getInputFormat();
    plugin_chain.configure(format.nSamplesPerSec, engine.maxBlockSize());

    mitmegy::MessageHandler handler(engine, plugin_chain, registry);

    engine.start();
    handler.start();

    g_running.wait(true);

    handler.stop();
    engine.stop();

    return 0;
  }
  catch(const std::exception& e)
  {
    std::cout << e.what();
    return 1;
  }
}
