#pragma once

#include <atomic>
#include <string>
#include <string_view>
#include <vector>

namespace mitmegy
{

struct Endpoint
{
  std::wstring name;
  std::wstring id;
};

// COM must be initialized on the calling thread.
void enumerateRenderEndpoints(std::vector<Endpoint>& endpoints);

bool isAudioSink(const Endpoint& endpoint);

const Endpoint* findAudioSink(
  const std::vector<Endpoint>& endpoints);

const Endpoint* findOutputEndpoint(
  const std::vector<Endpoint>& endpoints,
  std::wstring_view filter);

void runAudioBridge(
  const Endpoint& audio_sink,
  const Endpoint& output,
  const std::atomic_bool& running);

} // namespace mitmegy
