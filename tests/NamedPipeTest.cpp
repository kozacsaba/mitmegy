#include "protocol/NamedPipe.hpp"
#include "protocol/Frame.hpp"

#ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
  #define NOMINMAX
#endif

#include <Windows.h>

#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <exception>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <thread>
#include <utility>
#include <vector>

using namespace mitmegy;
using namespace std::chrono_literals;

namespace
{

struct ConnectionPair
{
  NamedPipeConnection server;
  NamedPipeConnection client;
};

std::wstring uniquePipeName(std::wstring_view name)
{
  static std::atomic_uint64_t counter{0};

  std::wstring result = L"mitmegy.test.";
  result += std::to_wstring(GetCurrentProcessId());
  result += L".";
  result += name;
  result += L".";
  result += std::to_wstring(counter.fetch_add(1));

  return result;
}

ConnectionPair connectPair(std::wstring_view name)
{
  NamedPipeServer server{name};

  std::optional<NamedPipeConnection> serverConnection;
  std::exception_ptr serverError;

  std::thread acceptThread([&]
  {
    try
    { serverConnection = server.accept(); }
    catch (...)
    { serverError = std::current_exception(); }
  });

  std::optional<NamedPipeConnection> clientConnection;

  try
  {
    for (int i = 0; i < 1000 && !clientConnection; ++i)
    {
      clientConnection = NamedPipeClient::connect(name);

      if (!clientConnection)
        std::this_thread::sleep_for(1ms);
    }
  }
  catch (...)
  {
    server.cancel();
    acceptThread.join();
    throw;
  }

  if (!clientConnection)
    server.cancel();

  acceptThread.join();

  if (serverError)
    std::rethrow_exception(serverError);

  assert(serverConnection);
  assert(clientConnection);

  return {
    std::move(*serverConnection),
    std::move(*clientConnection)
  };
}

std::vector<uint8_t> makePayload(std::size_t size)
{
  std::vector<uint8_t> data(size);

  for (std::size_t i = 0; i < data.size(); ++i)
    data[i] = static_cast<uint8_t>((i * 131 + 17) & 0xff);

  return data;
}

std::vector<uint8_t> readExactly(NamedPipeConnection& connection, std::size_t size)
{
  std::vector<uint8_t> result(size);

  std::size_t offset = 0;
  while (offset < size)
  {
    auto read = connection.readSome(
      std::span<uint8_t>{result}.subspan(offset));

    assert(read);
    assert(*read > 0);

    offset += *read;
  }

  return result;
}

void rethrow(std::exception_ptr error)
{
  if (error)
    std::rethrow_exception(error);
}

Frame makeFrame(std::size_t type, std::size_t id, std::size_t payloadSize)
{
  Frame frame;

  frame.header.version = VERSION;
  frame.header.msg_type = type;
  frame.header.id = id;
  frame.payload = makePayload(payloadSize);
  frame.header.length = frame.payload.size();

  return frame;
}

void assertFramesEqual(const Frame& a, const Frame& b)
{
  assert(a.header.version == b.header.version);
  assert(a.header.msg_type == b.header.msg_type);
  assert(a.header.length == b.header.length);
  assert(a.header.id == b.header.id);
  assert(a.payload == b.payload);
}

} // namespace

// MARK: NamedPipe tests

void test_empty_pipe_name()
{
  bool serverThrew = false;

  try
  { [[maybe_unused]] NamedPipeServer server{L""}; }
  catch (const std::invalid_argument&)
  { serverThrew = true; }

  assert(serverThrew);

  bool clientThrew = false;

  try
  {
    [[maybe_unused]] auto connection =
      NamedPipeClient::connect(L"");
  }
  catch (const std::invalid_argument&)
  { clientThrew = true; }

  assert(clientThrew);
}

void test_connect_without_server()
{
  const auto name = uniquePipeName(L"no-server");
  auto connection = NamedPipeClient::connect(name);
  assert(!connection);
}

void test_connection_state()
{
  const auto name = uniquePipeName(L"state");
  auto pair = connectPair(name);

  assert(pair.server.isOpen());
  assert(pair.client.isOpen());

  pair.client.cancel();

  assert(pair.server.isOpen());
  assert(!pair.client.isOpen());
}

void test_connection_move()
{
  const auto name = uniquePipeName(L"move");
  auto pair = connectPair(name);

  NamedPipeConnection moved{std::move(pair.client)};

  assert(!pair.client.isOpen());
  assert(moved.isOpen());

  NamedPipeConnection assigned;
  assigned = std::move(moved);

  assert(!moved.isOpen());
  assert(assigned.isOpen());
}

void test_client_to_server()
{
  const auto name = uniquePipeName(L"client-server");
  auto pair = connectPair(name);

  const std::vector<uint8_t> data{0x00, 0x01, 0x7f, 0x80, 0xfe, 0xff};

  std::vector<uint8_t> received;
  std::exception_ptr readerError;

  std::thread reader([&]
  {
    try
    { received = readExactly(pair.server, data.size()); }
    catch (...)
    { readerError = std::current_exception(); }
  });

  const bool written = pair.client.writeAll(std::span<const uint8_t>{data});

  reader.join();
  if (readerError)
    std::rethrow_exception(readerError);

  assert(written);
  assert(received == data);
}

void test_server_to_client()
{
  const auto name = uniquePipeName(L"server-client");
  auto pair = connectPair(name);

  const std::vector<uint8_t> data{10, 20, 30, 40, 50, 60};

  std::vector<uint8_t> received;
  std::exception_ptr readerError;

  std::thread reader([&]
  {
    try
    { received = readExactly(pair.client, data.size()); }
    catch (...)
    { readerError = std::current_exception(); }
  });

  const bool written = pair.server.writeAll(std::span<const uint8_t>{data});

  reader.join();
  if (readerError)
    std::rethrow_exception(readerError);

  assert(written);
  assert(received == data);
}

void test_cancelled_connection_rejects_write()
{
  const auto name = uniquePipeName(L"cancel-write");
  auto pair = connectPair(name);

  pair.client.cancel();

  const std::vector<uint8_t> data{1, 2, 3};
  assert(!pair.client.writeAll(std::span<const uint8_t>{data}));
}

void test_cancel_unblocks_accept()
{
  const auto name = uniquePipeName(L"cancel-accept");
  NamedPipeServer server{name};

  std::optional<NamedPipeConnection> result;
  std::exception_ptr error;

  std::thread thread([&]
  {
    try
    { result = server.accept(); }
    catch (...)
    { error = std::current_exception(); }
  });

  std::this_thread::sleep_for(20ms);

  server.cancel();
  thread.join();
  rethrow(error);
  assert(!result);
}

void test_cancel_unblocks_read()
{
  const auto name = uniquePipeName(L"cancel-read");
  auto pair = connectPair(name);

  std::optional<std::size_t> result;
  std::exception_ptr error;

  std::thread reader([&]
  {
    try
    {
      std::vector<uint8_t> buffer(1024);
      result = pair.server.readSome(std::span<uint8_t>{buffer});
    }
    catch (...)
    { error = std::current_exception(); }
  });

  std::this_thread::sleep_for(20ms);

  pair.server.cancel();
  reader.join();
  rethrow(error);

  assert(!result);
  assert(!pair.server.isOpen());
}

void test_large_transfer()
{
  constexpr std::size_t DATA_SIZE = 4 * 1024 * 1024;

  const auto name = uniquePipeName(L"large");
  auto pair = connectPair(name);

  const auto data = makePayload(DATA_SIZE);

  bool writeResult = false;
  std::exception_ptr writerError;

  std::thread writer([&]
  {
    try
    { writeResult = pair.client.writeAll(std::span<const uint8_t>{data}); }
    catch (...)
    { writerError = std::current_exception(); }
  });

  const auto received = readExactly(pair.server, data.size());
  writer.join();
  rethrow(writerError);

  assert(writeResult);
  assert(received == data);
}

// MARK: Framer tests

void test_frame_roundtrip()
{
  const Frame original = makeFrame(42, 1234, 1024);
  const auto bytes = serialiseFrame(original);
  
  Framer framer{};
  framer.pushBytes(bytes);
  auto result = framer.getFrame();
  assert(result);

  assertFramesEqual(*result, original);
  assert(!framer.getFrame());
}

void test_fragmented_frame()
{
  const Frame original = makeFrame(7, 999, 4096);
  const auto bytes = serialiseFrame(original);

  Framer framer{};
  for (uint8_t byte : bytes)
    framer.pushBytes(std::vector<uint8_t>{byte});

  auto result = framer.getFrame();
  assert(result);

  assertFramesEqual(*result, original);
  assert(!framer.getFrame());
}

void test_multiple_frames()
{
  const Frame first = makeFrame(10, 100, 123);
  const Frame second = makeFrame(20, 200, 456);

  auto stream = serialiseFrame(first);
  const auto secondBytes = serialiseFrame(second);
  stream.insert(stream.end(), secondBytes.begin(), secondBytes.end());

  Framer framer{};
  framer.pushBytes(stream);

  auto firstResult = framer.getFrame();
  auto secondResult = framer.getFrame();
  auto thirdResult = framer.getFrame();

  assert(firstResult);
  assert(secondResult);
  assert(!thirdResult);

  assertFramesEqual(*firstResult, first);
  assertFramesEqual(*secondResult, second);
}

// MARK: Pipe + Framer integration

void test_frame_over_pipe()
{
  const auto name = uniquePipeName(L"frame");
  auto pair = connectPair(name);

  const Frame original = makeFrame(123, 456, 8192);
  const auto bytes = serialiseFrame(original);

  bool writeResult = false;
  std::exception_ptr writerError;

  std::thread writer([&]
  {
    try
    { writeResult = pair.client.writeAll(std::span<const uint8_t>{bytes}); }
    catch (...)
    { writerError = std::current_exception(); }
  });

  Framer framer{};

  std::size_t receivedBytes = 0;
  while (receivedBytes < bytes.size())
  {
    std::vector<uint8_t> buffer(997);
    auto read = pair.server.readSome(std::span<uint8_t>{buffer});

    assert(read);
    assert(*read > 0);

    receivedBytes += *read;
    buffer.resize(*read);
    framer.pushBytes(buffer);
  }

  writer.join();
  rethrow(writerError);
  assert(writeResult);

  auto result = framer.getFrame();
  assert(result);

  assertFramesEqual(*result, original);
  assert(!framer.getFrame());
}

void test_large_frame_over_pipe()
{
  constexpr std::size_t PAYLOAD_SIZE = 4 * 1024 * 1024;
  const auto name = uniquePipeName(L"large-frame");
  auto pair = connectPair(name);
  const Frame original = makeFrame(321, 654, PAYLOAD_SIZE);
  const auto bytes = serialiseFrame(original);

  bool writeResult = false;
  std::exception_ptr writerError;

  std::thread writer([&]
  {
    try
    { writeResult = pair.client.writeAll(std::span<const uint8_t>{bytes}); }
    catch (...)
    { writerError = std::current_exception(); }
  });

  Framer framer{};

  std::size_t receivedBytes = 0;
  while (receivedBytes < bytes.size())
  {
    std::vector<uint8_t> buffer(4093);
    auto read = pair.server.readSome(std::span<uint8_t>{buffer});

    assert(read);
    assert(*read > 0);

    receivedBytes += *read;
    buffer.resize(*read);
    framer.pushBytes(buffer);
  }

  writer.join();
  rethrow(writerError);
  assert(writeResult);

  auto result = framer.getFrame();
  assert(result);

  assertFramesEqual(*result, original);
  assert(!framer.getFrame());
}

void test_multiple_frames_over_pipe()
{
  const auto name = uniquePipeName(L"multiple-frames");
  auto pair = connectPair(name);
  const Frame first = makeFrame(1, 101, 5000);
  const Frame second = makeFrame(2, 102, 7000);
  const Frame third = makeFrame(3, 103, 3000);
  auto stream = serialiseFrame(first);

  {
    const auto bytes = serialiseFrame(second);
    stream.insert(stream.end(), bytes.begin(), bytes.end());
  }

  {
    const auto bytes = serialiseFrame(third);
    stream.insert(stream.end(), bytes.begin(), bytes.end());
  }

  bool writeResult = false;
  std::exception_ptr writerError;

  std::thread writer([&]
  {
    try
    { writeResult = pair.client.writeAll(std::span<const uint8_t>{stream}); }
    catch (...)
    { writerError = std::current_exception(); }
  });

  Framer framer{};
  std::vector<Frame> frames;

  std::size_t receivedBytes = 0;
  while (receivedBytes < stream.size())
  {
    std::vector<uint8_t> buffer(731);
    auto read = pair.server.readSome(std::span<uint8_t>{buffer});

    assert(read);
    assert(*read > 0);

    receivedBytes += *read;
    buffer.resize(*read);
    framer.pushBytes(buffer);

    while (auto frame = framer.getFrame())
      frames.push_back(std::move(*frame));
  }

  writer.join();
  rethrow(writerError);
  assert(writeResult);

  while (auto frame = framer.getFrame())
    frames.push_back(std::move(*frame));

  assert(frames.size() == 3);

  assertFramesEqual(frames[0], first);
  assertFramesEqual(frames[1], second);
  assertFramesEqual(frames[2], third);
}

// MARK: Main

int main()
{
  test_empty_pipe_name();
  test_connect_without_server();

  test_connection_state();
  test_connection_move();

  test_client_to_server();
  test_server_to_client();

  test_cancelled_connection_rejects_write();
  test_cancel_unblocks_accept();
  test_cancel_unblocks_read();

  test_large_transfer();

  test_frame_roundtrip();
  test_fragmented_frame();
  test_multiple_frames();

  test_frame_over_pipe();
  test_large_frame_over_pipe();
  test_multiple_frames_over_pipe();

  std::cout << "All named pipe and framer tests passed.\n";

  return 0;
}
