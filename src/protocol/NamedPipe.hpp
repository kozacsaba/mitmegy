#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace mitmegy
{

inline constexpr std::wstring_view PIPENAME = L"mitmegy.control";

class NamedPipeServer;
class NamedPipeClient;

class NamedPipeConnection
{
public:
  NamedPipeConnection() = default;
  ~NamedPipeConnection();

  NamedPipeConnection(const NamedPipeConnection&) = delete;
  NamedPipeConnection& operator=(const NamedPipeConnection&) = delete;

  NamedPipeConnection(NamedPipeConnection&& other) noexcept;
  NamedPipeConnection& operator=(NamedPipeConnection&& other) noexcept;

  // Returns however many bytes are currently read.
  // nullopt means disconnected or cancelled.
  std::optional<std::size_t> readSome(std::span<std::uint8_t> buffer);

  // Writes the complete span.
  // false means disconnected or cancelled.
  bool writeAll(std::span<const std::uint8_t> data);

  // Permanently cancels this connection and unblocks pending I/O.
  void cancel() noexcept;

  bool isOpen() const noexcept;

private:
  friend class NamedPipeServer;
  friend class NamedPipeClient;

  explicit NamedPipeConnection(void* handle) noexcept;

  void close() noexcept;

  void* handle_ = nullptr;
  bool cancelled_ = false;

  mutable std::mutex stateMutex_;
  std::mutex readMutex_;
  std::mutex writeMutex_;
};


class NamedPipeServer
{
public:
  explicit NamedPipeServer(std::wstring_view name);
  ~NamedPipeServer();

  NamedPipeServer(const NamedPipeServer&) = delete;
  NamedPipeServer& operator=(const NamedPipeServer&) = delete;

  // Blocks until a client connects.
  // nullopt means accept was cancelled.
  std::optional<NamedPipeConnection> accept();

  // Permanently cancels the server and interrupts a pending accept().
  void cancel() noexcept;

private:
  std::wstring path_;

  std::mutex stateMutex_;
  void* pendingHandle_ = nullptr;
  bool cancelled_ = false;
};


class NamedPipeClient
{
public:
  // Performs one connection attempt.
  // nullopt means no server instance is currently available.
  static std::optional<NamedPipeConnection> connect(std::wstring_view name);
};

} // namespace mitmegy
