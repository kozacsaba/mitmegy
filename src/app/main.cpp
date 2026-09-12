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
    // g_running.store(false, std::memory_order_relaxed);
    std::cout << "type quit" << std::endl;
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

void printHelp()
{
  std::cout << "Usage:\n"
               "  help -- print this\n"
               "  list [filter] -- print list of available plugins with optional filtering\n"
               "  chain -- print list of plugin instances in use\n"
               "  remove / rm <ID> -- remove the plugin instance with the runtime id ID (use chain to see ID of instances)\n"
               "  add <idx> <name> -- adds a new instance of a plugin to the chain at the index idx."
                  " Use -1 to insert at the end. Use <name> to find the plugin.\n"
               "  bypass <ID> -- bypess the plugin instance with the runtim id <ID>\n"
               "  stop / exit / quit -- stop the application\n"
            << std::endl;
}

enum class IOCommandType
{
  help,
  list,
  chain,
  remove,
  add,
  bypass,
  stop,

  error
};

struct IOCommandTask
{
  IOCommandType type;
  int index;
  std::string msg;
};

std::string getNextWord(std::string& src)
{
  const auto pos = src.find(' ');
  const std::string word = src.substr(0, pos);
  src = src.substr(pos+1);
  return word;
}

IOCommandType getIOCommandType(std::string& input)
{
  using Type = IOCommandType;

  const std::string command = getNextWord(input);
  if(command == "help")
    return Type::help;
  else if (command == "list")
    return Type::list;
  else if(command == "chain")
    return Type::chain;
  else if(command == "remove" || command == "rm")
    return Type::remove;
  else if (command == "add")
    return Type::add;
  else if (command == "bypass")
    return Type::bypass;
  else if (command == "stop" || command == "exit" || command == "quit")
    return Type::stop;
  else
    return Type::help;
}

IOCommandTask getIOCommand(const std::string input)
{
  using Type = IOCommandType;

  std::string line = input;
  Type type = getIOCommandType(line);

  IOCommandTask task;
  task.type = type;

  switch(type)
  {
    case Type::list:
      task.msg = line;
      return task;

    case Type::bypass:
    case Type::remove:
      task.index = std::stoi(getNextWord(line));
      return task;

    case Type::add:
      task.index = std::stoi(getNextWord(line));
      task.msg = line;
      return task;

    case Type::help:
    case Type::chain:
    case Type::stop:
    case Type::error:
      return task;
  }
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

    // MARK: Prepare Processor

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

    // MARK: Prepare Plugin registry

    mitmegy::PluginRegistry registry;
    std::cout << "Scanning plugins" << std::endl;
    registry.scan();
    std::cout << std::endl;
    auto plugins = registry.plugins().getTypes();

    // MARK: Launch Processor

    mitmegy::AudioEngine engine(*audio_sink, *output, process);
    const auto& format = engine.inputFormat();
    plugin_chain.configure(format.nSamplesPerSec, engine.maxBlockSize());
    printFormat(format);

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
