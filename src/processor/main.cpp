#include "vsthost/PluginChain.hpp"
#include "vsthost/PluginRegistry.hpp"

#include "Engine.hpp"

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
    // std::cout << "type quit" << std::endl;
    return TRUE;
  }

  return FALSE;
}

bool containsInsensitive(std::string_view text, std::string_view substring)
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

std::string getNextWord(std::string& src)
{
  const auto pos = src.find(' ');
  const std::string word = src.substr(0, pos);
  src = src.substr(pos+1);
  return word;
}

void printPluginList(
  const juce::Array<juce::PluginDescription>& plugins,
  const std::string& filter = "")
{
  const bool do_filter = !filter.empty();

  for (int i = 0; i < plugins.size(); ++i)
  {
    const auto& plugin = plugins.getReference(i);
    if(do_filter && !containsInsensitive(plugin.name.toStdString(), filter))
      continue;

    std::cout
      << "[" << i << "] "
      << plugin.name
      << " - "
      << plugin.manufacturerName
      << '\n';
  }
}

juce::PluginDescription& findPluginDescriptor(
  juce::Array<juce::PluginDescription>& plugins,
  const std::string& keyword)
{
  auto it = std::find_if(
    plugins.begin(), plugins.end(), 
    [&](const juce::PluginDescription& elem)
    {
      return containsInsensitive(elem.name.toStdString(), keyword);
    });

  if(it == plugins.end())
    throw std::runtime_error("No plugins found matching the keyword " + keyword);

  return *it;
}

} // namespace

int wmain(int argc, wchar_t* argv[])
{
  try
  {
    SetConsoleCtrlHandler(consoleHandler, TRUE);

    mitmegy::Engine engine{};

    mitmegy::PluginChain plugin_chain;
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





    
    // MARK: Prepare Plugin registry

    mitmegy::PluginRegistry registry;
    std::cout << "Scanning plugins" << std::endl;
    registry.scan();
    std::cout << std::endl;
    auto plugins = registry.plugins().getTypes();

    // MARK: Launch Processor



    std::thread t_engine([&]()
    {
      std::wcout << L"Started Plugin Host. Enter \"help\" for available"
                    L"commands, press Ctrl+C to stop.\n";
      engine.run(g_running);
      printStats(engine.stats());
    });

    // MARK: VST SELECT

    std::thread t_io([&]()
    {
      using Type = IOCommandType;
      std::string line;

      while(g_running.load())
      {
        std::getline(std::cin, line);
        IOCommandTask task = getIOCommand(line);

        try
        {
          switch(task.type)
          {
            case Type::help:
              printHelp();
              break;
            case Type::list:
              printPluginList(plugins, task.msg);
              break;
            case Type::chain:
            {
              for(auto plugin : plugin_chain.getChain())
              {
                std::cout << "[" << plugin->runtime_id_ << "]";
                if(plugin->bypassed_)
                  std::cout << " [bypassed] ";
                std::cout << " - " << plugin->instance_->getName();
                std::cout << std::endl;
              }
              break;
            }
            case Type::remove:
              plugin_chain.remove(task.index);
              break;
            case Type::add:
            {
              auto& desc = findPluginDescriptor(plugins, task.msg);
              if (task.index < -1 || task.index >= plugins.size())
                throw std::runtime_error("Invalid plugin index");
              auto plugin = registry.createInstance(desc);
              plugin_chain.add(task.index, std::move(plugin));
              std::cout << "Inserted plugin " << desc.name << " at index" << task.index << std::endl;
              break;
            }
            case Type::bypass:
              plugin_chain.plugin_id(task.index)->bypassed_ = true;
              break;
            case Type::stop:
              std::cout << "\nStopping" << std::endl;
              g_running = false;
              g_running.notify_all();
              break;
            case Type::error:
              std::cout << "Invalid input: " << task.msg << std::endl;
              break;
          }
        }
        catch(const std::exception& e)
        {
          std::cout << "Error: " << e.what() << std::endl;
        }
      }
    });

    g_running.wait(true);

    if(t_io.joinable())
      t_io.join();

    if(t_engine.joinable())
      t_engine.join();

    return 0;
  }
  catch(const std::exception& e)
  {
    std::cout << e.what();
    return 1;
  }
}
