# User-mode audio engine — implementation order

Size: **S / M / L / XL**  
Priority: **P0 / P1 / P2 / P3**

1. **Define internal processing format — M / P0**
   - Define the engine's float sample representation, channel layout, sample rate and processing block semantics.
   - Use JUCE-compatible representations where practical, primarily `juce::AudioBuffer<float>`.

2. **Add FIFO / ring buffer — M / P0**
   - Add a preallocated producer/consumer FIFO between WASAPI capture and render.
   - Store frames independently of WASAPI packet boundaries.
   - Support fill-level queries, overflow detection and underflow detection.

3. **Add input/output sample conversion — S / P0**
   - Convert captured PCM into the internal float format.
   - Convert processed float audio into the format required by the render path.









4. **Introduce processing callback / processing stage — S–M / P0**
   - Add a transport-independent processing interface between input conversion and output.
   - Allow processing to operate on internal float blocks.

5. **Implement passthrough processing — S / P0**
   - Run audio through the new processing path unchanged.
   - Verify that the FIFO, conversion and block architecture works correctly.

6. **Add emergency output safety guard — S / P0**
   - Detect NaN/Inf and obviously invalid output.
   - Provide emergency mute or safe bypass behavior.

7. **Add startup FIFO prefill — S / P1**
   - Capture audio until a configurable target FIFO level is reached before beginning normal render consumption.

8. **Add basic transport statistics — S / P1**
   - Track captured/rendered frames, FIFO fill level, min/max fill, underruns, overruns, dropped frames and capture discontinuities.

9. **Integrate JUCE — M / P1**
   - Add the required JUCE modules through CMake.
   - Use JUCE for plugin hosting and audio-processing abstractions without replacing the custom WASAPI transport.

10. **Host and process through one VST3 plugin — M / P1**
    - Discover/load a VST3 plugin.
    - Configure it with the engine sample rate, block size and channel layout.
    - Pass internal audio blocks through `processBlock()`.

11. **Add simple serial plugin chain — M / P1**
    - Support multiple plugins processed sequentially.
    - Add basic plugin enable/bypass and lifecycle handling.

12. **Add basic output limiter — S–M / P1**
    - Add a simple safety limiter after plugin processing.
    - Prevent excessive plugin output from reaching the physical device.

13. **Convert WASAPI transport to event-driven operation — M / P1**
    - Replace polling and `Sleep(1)` with WASAPI event notifications.
    - Add clean event-driven capture, render and shutdown handling.

14. **Add clock-drift measurement — S–M / P1**
    - Observe FIFO occupancy over time.
    - Calculate smoothed deviation from the target FIFO level.
    - Collect enough information to determine actual producer/consumer clock mismatch.

15. **Add adaptive resampling / drift correction — M / P2**
    - Integrate a lightweight external resampling library.
    - Control its ratio using the FIFO occupancy error.
    - Apply slow, bounded corrections to keep FIFO occupancy near its target.

16. **Improve WASAPI buffer configuration — S–M / P2**
    - Remove the fixed ~100 ms buffer configuration.
    - Query endpoint periods/capabilities and choose suitable stable buffer sizes.

17. **Add clean device-loss handling — S–M / P2**
    - Detect invalidated, removed or disabled endpoints.
    - Stop the engine safely and report the failure instead of crashing or hanging.

18. **Add exclusive-mode physical output — M–L / P3**
    - Allow the physical render endpoint to optionally operate in WASAPI exclusive mode.
    - Handle format negotiation and exclusive-mode buffer requirements.

19. **Add live device switching and hot-plug handling — L / P3**
    - Detect endpoint additions/removals.
    - Allow the physical output device to be changed while the application is running.

20. **Add sleep / wake handling — M–L / P3**
    - Detect suspend/resume transitions.
    - Shut down and restore affected audio resources safely.

21. **Add automatic device recovery — L / P3**
    - Recreate WASAPI clients after recoverable device failures.
    - Reset FIFO/resampler state, prefill and restart playback automatically.

22. **Optimize latency — M–L / P3**
    - Tune WASAPI buffers, FIFO target level, processing block sizes and resampler buffering.
    - Reduce end-to-end latency without compromising stability.

23. **Add plugin sandbox process — XL / P3**
    - Move third-party plugins into a separate process.
    - Implement real-time audio IPC, shared-memory transport, crash detection and plugin-host restart/recovery.

24. **Mix Format Compatibility**
    - Do not assume AudioSink's shared-mode mix format is always stereo 48 kHz IEEE float32
    - Validate the format at startup and later support other AudioSink PCM/mix formats through input conversion.
