#include "Engine.hpp"

#include <stdexcept>
#include <format>
#include <algorithm>
#include <cwctype>
#include <iostream>

namespace
{

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

mitmegy::Endpoint* findAudioSink(std::vector<mitmegy::Endpoint>& endpoints)
{
  const auto it = std::find_if(
    endpoints.begin(), endpoints.end(),
    isAudioSink);

  return it == endpoints.end()
    ? nullptr
    : &*it;
}

} // namespace

namespace mitmegy
{

Engine::Engine()
  : com_()
{
  if (!com_.ok()) 
    throw std::runtime_error(
      "CoInitializeEx failed: 0x" + 
      std::format("{:x}", (unsigned long)com_.result()));

  mitmegy::enumerateRenderEndpoints(endpoints_);

  audio_sink_ = findAudioSink(endpoints_);
  if (audio_sink_ == nullptr)
    throw std::runtime_error("ERROR: AudioSink endpoint is not available.");

  // temporarily select idx 0 as default.
  // TODO: save current state / snapshot
  if(&endpoints_[0] == audio_sink_)
    selectDevice(endpoints_[1].id);
  else
    selectDevice(endpoints_[0].id);
}

Engine::~Engine()
{
  stop();
}

void Engine::start()
{
  if(running_.exchange(true))
    return;

  if(engine_ == nullptr)
    return;

  thread_ = std::thread([this](){ engine_->run(running_); });
}

void Engine::stop()
{
  if(!running_.exchange(false))
    return;

  if(thread_.joinable())
    thread_.join();
}

const std::vector<mitmegy::Endpoint>& Engine::getEndpoints()
{
  return endpoints_;
}

bool Engine::selectDevice(const std::wstring& endpoint_id)
{
  auto it = std::find_if(
    endpoints_.begin(), endpoints_.end(), 
    [&](const mitmegy::Endpoint& elem)
    { 
      return elem.id == endpoint_id; 
    });

  if(it == endpoints_.end())
    return false;

  engine_ = nullptr;
  engine_ = std::make_unique<mitmegy::AudioEngine>(*audio_sink_, it);
  engine_->process_ = callback_;

  // TMP
  log(
    L"Selected output endpoint:\n"
    L"    " + it->name + L'\n'
    + L"    ID: " + it->id + L"\n\n"
    + L"Forwarding:\n"
    + L"    " + audio_sink_->name + L"\n"
    + L"        -> " + it->name + L"\n"
  );
}

const WAVEFORMATEX& Engine::getInputFormat() const
{
  if(!engine_)
    throw std::runtime_error("No active engine");

  return engine_->inputFormat();
}

SampleFormat Engine::inputSampleFormat() const
{
  if(!engine_)
    throw std::runtime_error("No active engine");

  return engine_->inputSampleFormat();
}

size_t Engine::maxBlockSize() const
{
  if(!engine_)
    throw std::runtime_error("No active engine");

  return engine_->maxBlockSize();
}

void Engine::setProcessCallback(ProcessCallback callback)
{
  callback_ = std::move(callback);
  if(engine_)
    engine_->process_ = callback;
}

void Engine::log(const std::string& msg)
{
  std::cout << msg << std::endl;
}

void Engine::log(const std::wstring& msg)
{
  std::wcout << msg << std::endl;
}

} // namespace mitmegy
