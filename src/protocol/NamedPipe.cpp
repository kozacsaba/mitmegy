#ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
  #define NOMINMAX
#endif

#include "NamedPipe.hpp"

#include <Windows.h>

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <system_error>
#include <utility>

namespace mitmegy
{

namespace
{

constexpr std::wstring_view PipePrefix = LR"(\\.\pipe\)";

std::wstring makePipePath(std::wstring_view name)
{
  if (name.empty())
    throw std::invalid_argument("named pipe name is empty");

  if (name.starts_with(PipePrefix))
    return std::wstring{name};

  std::wstring path{PipePrefix};
  path.append(name);
  return path;
}

[[noreturn]] void throwWindowsError(const char* operation, DWORD error)
{
  throw std::system_error(
    static_cast<int>(error),
    std::system_category(),
    operation);
}

[[noreturn]] void throwLastWindowsError(const char* operation)
{
  throwWindowsError(operation, GetLastError());
}

bool isDisconnectError(DWORD error)
{
  return error == ERROR_BROKEN_PIPE ||
         error == ERROR_PIPE_NOT_CONNECTED ||
         error == ERROR_NO_DATA ||
         error == ERROR_OPERATION_ABORTED;
}

HANDLE toHandle(void* handle)
{
  return static_cast<HANDLE>(handle);
}

class OverlappedOperation
{
public:
  OverlappedOperation()
  {
    overlapped_.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (overlapped_.hEvent == nullptr)
      throwLastWindowsError("CreateEventW");
  }

  ~OverlappedOperation()
  {
    CloseHandle(overlapped_.hEvent);
  }

  OVERLAPPED* get()
  {
    return &overlapped_;
  }

private:
  OVERLAPPED overlapped_{};
};

// Completes an overlapped operation after ReadFile/WriteFile returned either
// success or ERROR_IO_PENDING.
// Returns nullopt for disconnect/cancellation.
std::optional<DWORD> completeIo(
  HANDLE handle, OVERLAPPED* overlapped, BOOL started,
  DWORD startError, const char* operation)
{
  if (!started && startError != ERROR_IO_PENDING)
  {
    if (isDisconnectError(startError))
      return std::nullopt;

    throwWindowsError(operation, startError);
  }

  DWORD transferred = 0;

  if (!GetOverlappedResult(handle, overlapped, &transferred, TRUE))
  {
    const DWORD error = GetLastError();
    if (isDisconnectError(error))
      return std::nullopt;

    throwWindowsError(operation, error);
  }

  return transferred;
}

} // namespace

// NamedPipeConnection

NamedPipeConnection::NamedPipeConnection(void* handle) noexcept
  : handle_(handle)
{}

NamedPipeConnection::~NamedPipeConnection()
{
  close();
}

NamedPipeConnection::NamedPipeConnection(NamedPipeConnection&& other) noexcept
{
  std::lock_guard lock(other.stateMutex_);
  handle_ = std::exchange(other.handle_, nullptr);
  cancelled_ = std::exchange(other.cancelled_, true);
}

NamedPipeConnection& NamedPipeConnection::operator=(NamedPipeConnection&& other) noexcept
{
  if (this == &other)
    return *this;

  std::scoped_lock lock(stateMutex_, other.stateMutex_);

  if (handle_ != nullptr)
  {
    CancelIoEx(toHandle(handle_), nullptr);
    CloseHandle(toHandle(handle_));
  }

  handle_ = std::exchange(other.handle_, nullptr);
  cancelled_ = std::exchange(other.cancelled_, true);

  return *this;
}

bool NamedPipeConnection::isOpen() const noexcept
{
  std::lock_guard lock(stateMutex_);
  return handle_ != nullptr && !cancelled_;
}

std::optional<std::size_t> NamedPipeConnection::readSome(std::span<std::uint8_t> buffer)
{
  if (buffer.empty())
    return std::size_t{0};

  std::lock_guard readLock(readMutex_);
  OverlappedOperation operation;

  HANDLE handle;
  BOOL started;
  DWORD startError = ERROR_SUCCESS;

  { // lock state
    std::lock_guard stateLock(stateMutex_);

    if (handle_ == nullptr || cancelled_)
      return std::nullopt;

    handle = toHandle(handle_);
    const DWORD size = static_cast<DWORD>(
      std::min<std::size_t>(buffer.size(), std::numeric_limits<DWORD>::max()));

    started = ReadFile(handle, buffer.data(), size, nullptr, operation.get());
    if (!started)
      startError = GetLastError();
  } // lock state

  const auto transferred = completeIo(handle, operation.get(), started, startError, "ReadFile");
  if (!transferred || *transferred == 0)
  {
    cancel();
    return std::nullopt;
  }

  return static_cast<std::size_t>(*transferred);
}

bool NamedPipeConnection::writeAll(std::span<const std::uint8_t> data)
{
  std::lock_guard writeLock(writeMutex_);

  std::size_t offset = 0;
  while (offset < data.size())
  {
    OverlappedOperation operation;
    HANDLE handle;
    BOOL started;
    DWORD startError = ERROR_SUCCESS;

    const DWORD size = static_cast<DWORD>(
      std::min<std::size_t>(data.size() - offset, std::numeric_limits<DWORD>::max()));

    { // lock state
      std::lock_guard stateLock(stateMutex_);

      if (handle_ == nullptr || cancelled_)
        return false;

      handle = toHandle(handle_);

      started = WriteFile(handle, data.data() + offset, size, nullptr, operation.get());
      if (!started)
        startError = GetLastError();
    } // lock state

    const auto transferred = completeIo(handle, operation.get(), started, startError, "WriteFile");
    if (!transferred || *transferred == 0)
    {
      cancel();
      return false;
    }

    offset += *transferred;
  }

  return true;
}

void NamedPipeConnection::cancel() noexcept
{
  std::lock_guard lock(stateMutex_);

  if (cancelled_)
    return;

  cancelled_ = true;
  if (handle_ != nullptr)
    CancelIoEx(toHandle(handle_), nullptr);
}

void NamedPipeConnection::close() noexcept
{
  std::lock_guard lock(stateMutex_);

  if (handle_ == nullptr)
    return;

  cancelled_ = true;

  CancelIoEx(toHandle(handle_), nullptr);
  CloseHandle(toHandle(handle_));

  handle_ = nullptr;
}

// NamedPipeServer

NamedPipeServer::NamedPipeServer(std::wstring_view name)
    : path_(makePipePath(name))
{}

NamedPipeServer::~NamedPipeServer()
{
  cancel();
}

std::optional<NamedPipeConnection> NamedPipeServer::accept()
{
  { // lock state
    std::lock_guard lock(stateMutex_);

    if (cancelled_)
      return std::nullopt;
  } // lock state

  constexpr DWORD open_mode_flags = PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED;
  constexpr DWORD pipe_mode_flags = PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS;
  HANDLE pipe = CreateNamedPipeW(
    path_.c_str(), open_mode_flags, pipe_mode_flags,
    PIPE_UNLIMITED_INSTANCES, 0, 0, 0, nullptr);

  if (pipe == INVALID_HANDLE_VALUE)
    throwLastWindowsError("CreateNamedPipeW");

  OverlappedOperation operation;
  bool connectedImmediately = false;

  { // lock state
    std::lock_guard lock(stateMutex_);

    if (cancelled_)
    {
      CloseHandle(pipe);
      return std::nullopt;
    }

    pendingHandle_ = pipe;

    if (ConnectNamedPipe(pipe, operation.get()))
    {
      connectedImmediately = true;
    }
    else
    {
      const DWORD error = GetLastError();

      if (error == ERROR_PIPE_CONNECTED)
      {
        connectedImmediately = true;
      }
      else if (error != ERROR_IO_PENDING)
      {
        pendingHandle_ = nullptr;
        CloseHandle(pipe);
        throwWindowsError("ConnectNamedPipe", error);
      }
    }

    if (connectedImmediately)
      pendingHandle_ = nullptr;
  } // lock state

  if (!connectedImmediately)
  {
    DWORD transferred = 0;

    const BOOL success = GetOverlappedResult(pipe, operation.get(), &transferred, TRUE);
    const DWORD error = success ? ERROR_SUCCESS : GetLastError();

    { // lock state
      std::lock_guard lock(stateMutex_);

      if (pendingHandle_ == pipe)
        pendingHandle_ = nullptr;
    } // lock state

    if (!success)
    {
      CloseHandle(pipe);

      if (error == ERROR_OPERATION_ABORTED)
        return std::nullopt;

      throwWindowsError("ConnectNamedPipe", error);
    }
  }

  return NamedPipeConnection{pipe};
}

void NamedPipeServer::cancel() noexcept
{
  std::lock_guard lock(stateMutex_);

  if (cancelled_)
    return;

  cancelled_ = true;

  if (pendingHandle_ != nullptr)
    CancelIoEx(toHandle(pendingHandle_), nullptr);
}

// NamedPipeClient

std::optional<NamedPipeConnection> NamedPipeClient::connect(std::wstring_view name)
{
  const std::wstring path = makePipePath(name);

  HANDLE pipe = CreateFileW(
    path.c_str(), GENERIC_READ | GENERIC_WRITE,
    0, nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);

  if (pipe == INVALID_HANDLE_VALUE)
  {
    const DWORD error = GetLastError();
    if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND || error == ERROR_PIPE_BUSY)
      return std::nullopt;

    throwWindowsError("CreateFileW", error);
  }

  return NamedPipeConnection{pipe};
}

} // namespace mitmegy
