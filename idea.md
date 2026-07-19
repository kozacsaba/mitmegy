# Concise conclusion

WASAPI lets your user-mode app read/write audio to endpoints, but it can’t magically intercept other apps’ audio unless you control where they output.

To process system-wide audio like FxSound, you need either:

an APO (deep integration but hostile to arbitrary VST hosting/UI in-process), or

a virtual audio driver that creates a new playback endpoint users set as default.

With a virtual driver, your user-mode app becomes the bridge:

apps → virtual endpoint → your user-mode engine (VST host) → selected real hardware endpoint.

The driver should stay thin (just a virtual device + streaming), while all complex logic (VST hosting, UI, routing, buffering, crash recovery) stays in user mode.

If you change anything in the driver package (INF/device IDs/names), you must get your own signed package (Microsoft signs the driver package, not your app EXE). They don’t need your source; you submit the driver package (typically as a CAB) and receive a Microsoft-signed package back.

Short plan to build your app (FxSound VAD fork + Microsoft signing)

## Phase 0 — Define the architecture

Virtual driver exposes a system playback device: “MyApp Speakers”.

Your user-mode service/app:

captures the stream coming out of the virtual device (commonly via a paired capture endpoint / cable-style design),

runs a VST chain,

renders to a user-selected real output endpoint (USB interface / speakers / etc.).

Optional: separate UI app that talks to the engine service (recommended).

## Phase 1 — Fork and minimally modify the FxSound virtual driver

Goal: keep kernel risk low by changing only “identity/packaging”, not streaming logic.

Fork FxSound’s driver repo.

Change INF / hardware IDs / device names / GUIDs so it’s your device and doesn’t conflict with FxSound.

Ensure the driver still exposes the endpoints you need (render, and if applicable the paired capture path used for pickup).

Build in WDK/VS; verify you can install it in test mode first.

## Phase 2 — Build the user-mode audio engine (core of the product)

Implement a WASAPI bridge:

open virtual-device side for input,

open selected hardware device for output,

connect them with a ring buffer/FIFO.

Handle clock drift (FIFO target level + light adaptive resampling if needed).

Add a minimal always-safe fallback: bypass / mute / limiter so a bad plugin can’t explode users’ ears.

Host plugins:

Start with JUCE AudioPluginFormatManager loading VST3.

Strongly consider a plugin sandbox process (plugins crash a lot; keep audio engine alive).

## Phase 3 — Build UI

Device selection (choose real output device).

Plugin chain management (scan, add/remove, reorder, bypass).

Presets/state saving.

## Phase 4 — Hardening and testing

Long-run playback (hours), sleep/wake, device hotplug, sample-rate changes.

Buffer underrun/overrun monitoring, auto-recovery.

VM testing for driver changes.

## Phase 5 — Microsoft signing

Produce a proper driver package from your build (INF/SYS/CAT…).

Create the submission artifact (commonly CAB for attestation signing).

Submit via Microsoft’s driver signing pipeline; download the Microsoft-signed package.

Bundle the signed driver package into your installer; ship app + driver as one product.

If you want the next step to be concrete: paste the FxSound driver repo structure / the endpoint names it creates (or link the specific fork commit), and I’ll tell you exactly which identifiers (INF/device IDs/strings/GUIDs) are the “necessary changes” to make it your own while keeping the kernel logic untouched.
