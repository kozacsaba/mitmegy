#include "mitmegy/AudioFifo.hpp"

#include <array>
#include <cassert>
#include <iostream>

using mitmegy::AudioFifo;

namespace
{

void testInterleavedWriteRead()
{
  AudioFifo fifo(2, 8);

  const std::array<float, 8> input{
    1.0f, 10.0f,
    2.0f, 20.0f,
    3.0f, 30.0f,
    4.0f, 40.0f
  };

  assert(fifo.writeInterleaved(input, 4) == 4);
  assert(fifo.readableFrames() == 4);
  assert(fifo.writableFrames() == 4);

  std::array<float, 8> output{};

  assert(fifo.readInterleaved(output, 4) == 4);
  assert(output == input);

  assert(fifo.readableFrames() == 0);
  assert(fifo.writableFrames() == 8);
}

void testPlanarViews()
{
  AudioFifo fifo(2, 8);

  {
    auto view = fifo.acquireWrite(4);

    assert(view);
    assert(view.frames() == 4);
    assert(view.channels() == 2);

    auto left = view.channel(0);
    auto right = view.channel(1);

    for (size_t i = 0; i < 4; ++i)
    {
      left[i] = static_cast<float>(i + 1);
      right[i] = static_cast<float>((i + 1) * 10);
    }

    view.commit(4);

    assert(!view);
  }

  {
    auto view = fifo.acquireRead(4);

    assert(view);
    assert(view.frames() == 4);

    const auto left = view.channel(0);
    const auto right = view.channel(1);

    for (size_t i = 0; i < 4; ++i)
    {
      assert(left[i] == static_cast<float>(i + 1));
      assert(right[i] == static_cast<float>((i + 1) * 10));
    }

    view.consume(4);

    assert(!view);
  }

  assert(fifo.readableFrames() == 0);
}

void testWrapAround()
{
  AudioFifo fifo(1, 5);

  const std::array<float, 4> first{
    1.0f, 2.0f, 3.0f, 4.0f
  };

  assert(fifo.writeInterleaved(first, 4) == 4);

  std::array<float, 3> discarded{};
  assert(fifo.readInterleaved(discarded, 3) == 3);

  const std::array<float, 4> second{
    5.0f, 6.0f, 7.0f, 8.0f
  };

  assert(fifo.writeInterleaved(second, 4) == 4);
  assert(fifo.readableFrames() == 5);

  std::array<float, 5> output{};
  assert(fifo.readInterleaved(output, 5) == 5);

  const std::array<float, 5> expected{
    4.0f, 5.0f, 6.0f, 7.0f, 8.0f
  };

  assert(output == expected);
}

void testFullAndEmpty()
{
  AudioFifo fifo(1, 4);

  const std::array<float, 6> input{
    1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f
  };

  assert(fifo.writeInterleaved(input, 6) == 4);
  assert(fifo.readableFrames() == 4);
  assert(fifo.writableFrames() == 0);

  std::array<float, 6> output{};

  assert(fifo.readInterleaved(output, 6) == 4);
  assert(fifo.readableFrames() == 0);
  assert(fifo.writableFrames() == 4);

  assert(output[0] == 1.0f);
  assert(output[1] == 2.0f);
  assert(output[2] == 3.0f);
  assert(output[3] == 4.0f);
}

void testPartialCommit()
{
  AudioFifo fifo(1, 8);

  auto view = fifo.acquireWrite(6);

  assert(view);
  assert(view.frames() == 6);

  for (size_t i = 0; i < 6; ++i)
    view.channel(0)[i] = static_cast<float>(i + 1);

  view.commit(4);

  assert(!view);
  assert(fifo.readableFrames() == 4);
}

void testPartialConsume()
{
  AudioFifo fifo(1, 8);

  const std::array<float, 6> input{
    1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f
  };

  assert(fifo.writeInterleaved(input, 6) == 6);

  auto view = fifo.acquireRead(6);

  assert(view);
  assert(view.frames() == 6);

  view.consume(4);

  assert(!view);
  assert(fifo.readableFrames() == 2);

  std::array<float, 2> remaining{};
  assert(fifo.readInterleaved(remaining, 2) == 2);

  assert(remaining[0] == 5.0f);
  assert(remaining[1] == 6.0f);
}

} // namespace

int main()
{
  testInterleavedWriteRead();
  testPlanarViews();
  testWrapAround();
  testFullAndEmpty();
  testPartialCommit();
  testPartialConsume();

  std::cout << "AudioFifoTest passed\n";
  return 0;
}
