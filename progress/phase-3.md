## Current audio path

The user-mode bridge currently implements the following path:
    Applications
        → Windows Audio Engine
        → AudioSink virtual endpoint
        → WASAPI loopback capture
        → input format boundary
        → planar float FIFO
        → processing stage
        → output safety guard
        → output format boundary
        → WASAPI shared render
        → selected physical output device

The bridge is currently a command-line tool used to validate and develop the audio engine before GUI and VST hosting are added.

## Endpoint and WASAPI handling

The bridge:
- enumerates active Windows render endpoints;
- locates the `AudioSink` virtual endpoint;
- selects a physical output endpoint using a CLI substring filter;
- opens `AudioSink` using WASAPI shared-mode loopback capture;
- opens the selected physical output using WASAPI shared-mode render;
- lets Windows perform final output-device format conversion using:
  - `AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM`;
  - `AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY`;
- shuts down on `Ctrl+C`.

The currently observed AudioSink shared-mode mix format is:
    48000 Hz
    2 channels
    32-bit IEEE float
    WAVE_FORMAT_EXTENSIBLE
    ValidBitsPerSample = 32
    ChannelMask = 0x3

The current bridge explicitly supports this float format.
Support for other possible AudioSink mix formats is deferred to the compatibility task in `todo.md`.

## Internal audio format

The engine now has an explicit internal audio representation:
- sample type: `float`;
- planar / non-interleaved channel layout;
- frame-based indexing;
- sample rate and channel count inherited from the AudioSink stream;
- variable contiguous block sizes.

For stereo audio, internal storage is conceptually:
    channel 0: L0 L1 L2 L3 ...
    channel 1: R0 R1 R2 R3 ...

instead of WASAPI's interleaved representation:
    L0 R0 L1 R1 L2 R2 ...

The planar representation is deliberately compatible with zero-copy wrapping by `juce::AudioBuffer<float>` when JUCE is integrated.

## Audio FIFO

A dedicated `AudioFifo` implementation now separates capture from render.

The FIFO:
- is preallocated;
- performs no allocation during normal audio operation;
- stores planar `float` samples;
- is frame-based;
- supports multiple channels;
- is designed for single-producer / single-consumer use;
- tracks readable and writable frame counts;
- handles ring-buffer wrap-around;
- supports partial reads and writes;
- supports interleaved input/output convenience functions.

### Zero-copy views

The FIFO also exposes zero-copy planar access through:
    WriteView
    ReadView

A view exposes bounded `std::span<float>` channel regions covering the largest currently contiguous part of the ring.

Write usage is:
    acquireWrite()
        → write into channel spans
        → commit(number_of_frames)

Read usage is:
    acquireRead()
        → access/process channel spans
        → consume(number_of_frames)

`commit()` and `consume()` invalidate their view.
Only the committed or consumed prefix advances the FIFO, so callers may use fewer frames than the view exposes.
This mechanism is intended to allow future JUCE/VST processing directly on FIFO memory without an additional processing-buffer copy.

## Format conversion

Because the currently observed AudioSink stream already uses `float32`, no numeric sample conversion is currently required.

Capture performs:
    WASAPI interleaved float
        → planar float FIFO

Render performs:
    planar float FIFO
        → WASAPI interleaved float

These layout conversions are currently the only required input/output conversions.
The processing path itself remains planar, avoiding unnecessary copies between the FIFO and future DSP/VST processing.

## Processing stage

A processing stage now exists between FIFO input and physical rendering.
The current processing function is passthrough:

    FIFO ReadView
        → processAudio(...)
        → safety guard
        → render

`processAudio()` currently leaves the samples unchanged.
This establishes the insertion point for later DSP and VST3 processing without coupling processing code to WASAPI.
The current passthrough path has been tested audibly and forwards audio correctly.

## Output safety guard

Processed audio is checked before being sent to WASAPI.
If any sample in a processing block is non-finite:
- `NaN`;
- positive infinity;
- negative infinity;
the complete affected block is replaced with silence.
This prevents numerical failure inside future processing code or plugins from being forwarded directly to the physical output.
Finite values outside `[-1.0, +1.0]` are currently allowed through unchanged. Handling excessive but valid finite amplitudes belongs to the later limiter task.
The guard has been manually tested by deliberately injecting invalid floating-point values.

## Startup FIFO prefill

Rendering no longer starts against an empty FIFO.
The startup sequence is currently:
    start capture
        → fill FIFO
        → reach prefill target
        → prime WASAPI render buffer
        → start render
        → normal bridge loop

The temporary prefill target is 50 ms.
At 48 kHz this corresponds to:
    2400 frames

This eliminated the startup underrun observed before prefill was introduced.

## Transport statistics

The bridge currently tracks:
- captured frames;
- rendered frames;
- current FIFO fill;
- minimum observed FIFO fill;
- maximum observed FIFO fill;
- underruns;
- overruns;
- dropped frames;
- WASAPI capture discontinuities.

An underrun is counted when both:
- the WASAPI render buffer has no queued audio; and
- the FIFO has no readable frames.

An overrun is counted when a capture packet cannot be fully written into the FIFO.

## Current validation results

A representative short run after adding prefill produced:
    Captured frames: 487680
    Rendered frames: 487680
    FIFO fill: 0
    FIFO min fill: 0
    FIFO max fill: 2400
    Underruns: 0
    Overruns: 0
    Dropped frames: 0
    Capture discontinuities: 1

The important current properties are therefore:
- captured and rendered frame counts match;
- no frames are dropped;
- no FIFO overruns occur;
- startup prefill prevents the previously observed initial underrun;
- passthrough audio is correct;
- invalid floating-point processing output is prevented from reaching the device.

The single observed capture discontinuity currently occurs at startup and has not caused an observable transport failure.

## Unit testing

A root-level `tests` directory and CMake unit-test infrastructure have been added.
`AudioFifo` tests currently cover:
- interleaved write/read correctness;
- planar zero-copy views;
- FIFO wrap-around;
- full and empty behavior;
- partial writes and reads;
- partial `commit()`;
- partial `consume()`;
- readable/writable frame accounting.

The current FIFO unit tests pass.

## Current guarantees and limitations

The engine currently guarantees, for the supported AudioSink format:
- planar `float32` internal audio;
- preallocated FIFO transport;
- no normal-path FIFO allocation;
- packet-boundary-independent buffering;
- zero-copy access to internal planar processing blocks;
- correct interleaved ↔ planar layout conversion;
- explicit FIFO overrun/drop accounting;
- explicit render underrun accounting;
- startup buffering before physical playback begins;
- passthrough processing through the new processing stage;
- protection against `NaN` and infinite processing output.

It does **not yet** guarantee:
- compatibility with every possible AudioSink mix format;
- event-driven WASAPI operation;
- long-term clock synchronization between AudioSink and the physical device;
- adaptive resampling;
- tuned latency or WASAPI buffer sizes;
- finite-output limiting;
- device-loss recovery;
- VST3 hosting;
- plugin isolation.

Those remaining items are tracked in `todo.md`.
