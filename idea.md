# Concise conclusion

The application will use a render-only virtual audio driver based on the FxSound driver. The driver exposes a selectable Windows playback endpoint such as “MyApp Speakers”, behaves like a functioning PCM speaker device, advances a simulated playback position, and ultimately discards the rendered audio.

Applications send audio to this virtual endpoint through the normal Windows Audio Engine. The user-mode audio engine opens the same render endpoint using WASAPI loopback capture, receiving a copy of the endpoint mix before the virtual sink discards it. It then processes the PCM stream through a VST3 chain and renders the processed result to a user-selected physical output device.

Applications
    → Windows Audio Engine
    → MyApp virtual speaker endpoint
        ├→ WASAPI loopback → user-mode VST engine → real speakers
        └→ virtual driver sink → discarded

The driver should remain minimal and contain no DSP, VST hosting, UI or custom kernel-to-user audio transport. Complex audio processing, routing, buffering, device management and recovery remain in user mode.

Changes to the driver package—such as its INF, hardware ID, service name, filenames or product-specific GUIDs—require shipping a separately signed driver package. Microsoft signs the submitted driver package, not the user-mode application.

## Phase 1 — Define and build the virtual playback endpoint

### Architecture

The virtual driver exposes one render endpoint:

MyApp Speakers

It accepts standard uncompressed PCM audio from the Windows Audio Engine. Initially, preserve the FxSound-supported format configuration:

PCM using WAVEFORMATEXTENSIBLE
44.1 and 48 kHz
24-bit integer samples
2–8 channels
one render stream
no capture endpoint

The driver uses the existing PortCls/WaveCyclic implementation to provide:

Wave and Topology filter descriptions;
a render streaming pin;
endpoint and speaker topology;
cyclic buffer allocation;
stream state handling;
simulated playback position;
periodic timer/DPC notifications;
PnP and power-management behavior.

These are Windows audio-driver structures, not DSP filters. For the initial fork, preserve them unchanged.

The user-mode engine will later:

find the virtual render endpoint;
open it with WASAPI shared-mode loopback capture;
convert the captured samples to floating point;
process them through the VST3 chain;
render the processed audio to the selected physical endpoint.

No paired virtual recording endpoint or custom kernel communication channel is required.

### Fork the FxSound driver

Fork:

fxsound2/fxsound-driver

Use the Microsoft Virtual Audio Device Driver sample as supporting documentation for the inherited PortCls/WaveCyclic classes and topology.

For the first working fork, change only product and package identity:

INF provider and manufacturer names;
device description and endpoint-friendly name;
root-enumerated hardware and compatible IDs;
driver service name;
SYS, INF and CAT filenames where appropriate;
Wave and Topology subdevice display strings;
product-specific device/interface GUIDs;
version information, resources and copyright strings;
installer and development-installation scripts.

Do not change:

Windows-defined class, interface, category or node GUIDs;
CLSID_PortWaveCyclic or CLSID_PortTopology;
standard PCM and Kernel Streaming format GUIDs;
pin and node indices;
Wave/Topology physical connections;
stream, timer, DMA-position or PnP logic.

### Initial validation

Build the driver using Visual Studio and the Windows Driver Kit, then install it with Windows test signing enabled.

Validate that:

the virtual playback endpoint appears in Windows;
applications can select it as their output;
playback remains active without driver errors or stalled buffers;
no sound reaches the physical speakers;
Audacity can select:
Windows WASAPI
MyApp Speakers (loopback)
audio routed to the virtual endpoint can be recorded through loopback.

A successful Audacity recording proves that the virtual endpoint and its WASAPI loopback path are working before the custom user-mode engine is implemented.

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
