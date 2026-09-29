#pragma once

#include "mitmegy/AudioEngine.hpp"
#include "mitmegy/Wasapi.hpp"
#include "mitmegy/Com.hpp"

#include <memory>
#include <atomic>
#include <thread>

namespace mitmegy
{

class Engine
{
public:
  Engine();
  ~Engine();

  void start();
  void stop();
  bool isRunning() const { return running_; }

  const std::vector<mitmegy::Endpoint>& getEndpoints();
  bool selectDevice(const std::wstring& endpoint_id);

  const WAVEFORMATEX& getInputFormat() const;
  SampleFormat inputSampleFormat() const;
  size_t maxBlockSize() const;

  void setProcessCallback(ProcessCallback callback);

private:
  void log(const std::string& msg);
  void log(const std::wstring& msg);

  std::atomic<bool> running_;
  std::thread thread_;

  mitmegy::ComScope com_;

  std::vector<mitmegy::Endpoint> endpoints_;
  mitmegy::Endpoint* audio_sink_ = nullptr;
  std::unique_ptr<mitmegy::AudioEngine> engine_;

  ProcessCallback callback_;

}; // Engine

} // namespace mitmegy
