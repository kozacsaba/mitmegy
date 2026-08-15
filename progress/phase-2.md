# User-mode audio engine — current state and next steps

## Current state

The user-mode side currently has a working smoke-test bridge tool.

It:

- enumerates active Windows render endpoints;
- locates the virtual `AudioSink` endpoint;
- opens `AudioSink` using WASAPI shared-mode loopback capture;
- selects a physical output endpoint using a CLI substring filter;
- opens the selected output in WASAPI shared mode;
- forwards captured PCM audio directly to the selected output;
- terminates on `Ctrl+C`;
- reports basic information such as the selected endpoints, input format and
  dropped packets.

The current audio path is:

    Applications
        → Windows Audio Engine
        → AudioSink
        → WASAPI loopback capture
        → bridge tool
        → WASAPI shared render
        → physical output device

The implementation is intentionally only a smoke test. It proves that the
complete driver → loopback → user-mode → physical-device path works.

### Current intentional shortcuts / TODOs

The following parts are deliberately simplified and should not be considered
production-ready:

- **Polling instead of event-driven WASAPI**
  - The capture loop currently polls using `Sleep(1)`.
  - This should later use WASAPI event callbacks.

- **No intermediate FIFO / ring buffer**
  - Captured packets are currently forwarded directly to the render client.
  - Capture and render are therefore tightly coupled.

- **Packets are dropped when the output cannot accept them**
  - If the current render buffer has insufficient free space, the complete
    capture packet is discarded.
  - This is acceptable only for the smoke test.

- **Arbitrary large WASAPI buffer**
  - Both sides currently use a hard-coded ~100 ms buffer duration.
  - This was chosen for simplicity, not latency or correctness.

- **Windows performs output format conversion**
  - The render client is initialized using the AudioSink loopback format.
  - `AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM` and
    `AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY` are used to let Windows convert
    the stream to the physical endpoint's format.
  - There is currently no explicit internal processing format or resampler.

- **No clock-drift handling**
  - AudioSink and the physical device have independent effective clocks.
  - Their rates are assumed to match indefinitely, which is not safe for
    long-running playback.

- **No startup prefill**
  - Rendering begins without first building a safe amount of buffered audio.

- **Very limited runtime statistics**
  - Dropped packets are counted, but there is no proper tracking of:
    - FIFO fill level;
    - underruns;
    - overruns;
    - dropped frames;
    - capture discontinuities;
    - timing / drift.

- **No device-loss recovery**
  - Errors such as device removal, disable, format change or
    `AUDCLNT_E_DEVICE_INVALIDATED` currently terminate the bridge.

- **No real-time processing yet**
  - Samples are forwarded unchanged.
  - There is no float conversion, DSP or VST3 processing.

- **No output safety processing yet**
  - There is no limiter, emergency mute or other protection against invalid
    processing output.

- **No exclusive-mode output**
  - The physical endpoint currently uses shared mode only.
  - Exclusive mode can be considered later as an optional output mode.

- **No hot-plug / live device switching**
  - Endpoint selection happens only during startup.

- **No plugin isolation**
  - VST hosting and plugin sandboxing have not been implemented yet.

## Next milestone — robust audio transport

Before adding VST processing, the bridge should become a reliable real-time
audio transport.

### 1. Add a FIFO / ring buffer

Change the architecture to:

    WASAPI loopback capture
        → FIFO / ring buffer
        → WASAPI render
        → physical output

Capture becomes the producer and render becomes the consumer.

The FIFO should:

- be preallocated;
- avoid allocation on real-time paths;
- store audio frames independently of WASAPI packet boundaries;
- expose current fill level;
- detect overflow and underflow.

This should be the next concrete implementation step.

### 2. Add startup prefill

Before normal playback begins:

- capture enough audio to reach a target FIFO level;
- then start normal render consumption.

This provides scheduling headroom and reduces immediate underruns.

### 3. Convert capture and render to event-driven WASAPI

Replace polling and `Sleep(1)` with WASAPI event notification.

The capture and render sides should wake only when Windows indicates that work
is available.

This should reduce unnecessary latency, CPU usage and scheduling uncertainty.

### 4. Add proper transport statistics

Track at least:

- captured frames;
- rendered frames;
- FIFO fill level;
- minimum / maximum FIFO fill;
- underrun count;
- overrun count;
- dropped frames;
- capture discontinuities;
- device errors.

These statistics will be important when tuning buffering and drift correction.

### 5. Handle clock drift

AudioSink and the physical output device must be treated as independent clocks.

Even if both nominally run at 48 kHz, small clock differences can cause the
FIFO to slowly:

- fill until it overflows; or
- drain until it underruns.

Use FIFO occupancy as the main feedback signal.

Define a target FIFO level and continuously observe whether the average level is
moving away from that target.

### 6. Add adaptive resampling / rate correction

Introduce a lightweight resampler between capture and render.

Its ratio should be adjusted very slightly based on FIFO fill level so that the
long-term producer and consumer rates remain matched.

For example:

    capture
        → FIFO
        → adaptive resampler
        → render

The adjustment should be slow and small enough to be inaudible.

### 7. Define an internal processing format

Stop treating the AudioSink capture format as the entire application's native
format.

Introduce an explicit internal representation, most likely:

- floating-point samples;
- known channel layout;
- known block representation.

The path then becomes approximately:

    AudioSink PCM
        → input conversion
        → internal float audio
        → processing
        → output conversion / resampling
        → physical endpoint

This will also create the natural insertion point for VST processing.

### 8. Improve WASAPI buffer configuration

Remove the arbitrary 100 ms buffer duration.

Query the actual capabilities / shared-mode periodicity of the endpoints and
choose suitable values based on:

- stability;
- latency;
- processing block size;
- FIFO headroom.

Low latency is not the first priority; reliable operation should come first.

### 9. Add device-loss and stream recovery

Handle situations such as:

- physical output unplugged;
- device disabled;
- default device changed;
- endpoint format changed;
- sleep / wake;
- `AUDCLNT_E_DEVICE_INVALIDATED`.

Initially, recovery may simply stop the audio engine cleanly and report the
reason. Later it can automatically reopen devices where appropriate.

## After transport is stable — processing milestone

Once the transport can run reliably for long periods without FIFO drift,
underruns or dropped packets:

1. convert the internal stream to floating point;
2. introduce a processing callback / processing graph;
3. initially implement simple passthrough processing;
4. add a basic limiter / emergency mute path;
5. integrate JUCE;
6. load VST3 plugins using `AudioPluginFormatManager`;
7. process the stream through a plugin chain;
8. later consider running plugins in a separate sandbox process.

## Later work

After the core audio engine is stable:

- GUI;
- physical output selection;
- shared / exclusive output mode selection;
- plugin scanning and management;
- plugin reorder / bypass;
- preset and state persistence;
- live device switching;
- hot-plug handling;
- sleep / wake recovery;
- long-duration stress testing;
- latency tuning;
- plugin crash isolation.
