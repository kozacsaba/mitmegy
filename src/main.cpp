#include <windows.h>
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>
#include <wrl/client.h>

#include <algorithm>
#include <atomic>
#include <cwctype>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

using Microsoft::WRL::ComPtr;

namespace
{

struct Endpoint
{
    std::wstring name;
    std::wstring id;
    ComPtr<IMMDevice> device;
};

std::atomic<bool> g_running = true;

BOOL WINAPI ConsoleHandler(DWORD event)
{
    if (event == CTRL_C_EVENT)
    {
        g_running = false;
        return TRUE;
    }

    return FALSE;
}

void PrintHresult(const char* operation, HRESULT hr)
{
    std::cerr
        << operation
        << " failed: 0x"
        << std::hex
        << static_cast<unsigned long>(hr)
        << std::dec
        << '\n';
}

bool ContainsInsensitive(std::wstring_view text, std::wstring_view substring)
{
    if (substring.empty())
        return true;

    return std::search(
        text.begin(),
        text.end(),
        substring.begin(),
        substring.end(),
        [](wchar_t a, wchar_t b)
        {
            return std::towlower(a) == std::towlower(b);
        }) != text.end();
}

bool IsAudioSink(const Endpoint& endpoint)
{
    return ContainsInsensitive(endpoint.name, L"AudioSink");
}

HRESULT EnumerateRenderEndpoints(
    IMMDeviceEnumerator* enumerator,
    std::vector<Endpoint>& endpoints)
{
    ComPtr<IMMDeviceCollection> collection;

    HRESULT hr = enumerator->EnumAudioEndpoints(
        eRender,
        DEVICE_STATE_ACTIVE,
        collection.GetAddressOf());

    if (FAILED(hr))
        return hr;

    UINT count = 0;

    hr = collection->GetCount(&count);

    if (FAILED(hr))
        return hr;

    for (UINT i = 0; i < count; ++i)
    {
        ComPtr<IMMDevice> device;

        if (FAILED(collection->Item(i, device.GetAddressOf())))
            continue;

        ComPtr<IPropertyStore> properties;

        if (FAILED(device->OpenPropertyStore(
                STGM_READ,
                properties.GetAddressOf())))
        {
            continue;
        }

        PROPVARIANT name;
        PropVariantInit(&name);

        hr = properties->GetValue(
            PKEY_Device_FriendlyName,
            &name);

        if (FAILED(hr) || name.vt != VT_LPWSTR)
        {
            PropVariantClear(&name);
            continue;
        }

        LPWSTR id = nullptr;

        hr = device->GetId(&id);

        if (FAILED(hr))
        {
            PropVariantClear(&name);
            continue;
        }

        Endpoint endpoint;
        endpoint.name = name.pwszVal;
        endpoint.id = id;
        endpoint.device = device;

        endpoints.push_back(std::move(endpoint));

        CoTaskMemFree(id);
        PropVariantClear(&name);
    }

    return S_OK;
}

void PrintOutputEndpoints(const std::vector<Endpoint>& endpoints)
{
    std::wcout << L"Available output endpoints:\n\n";

    size_t index = 0;

    for (const auto& endpoint : endpoints)
    {
        // AudioSink is our input. Do not offer it as a physical output.
        if (IsAudioSink(endpoint))
            continue;

        std::wcout
            << L"["
            << index++
            << L"] "
            << endpoint.name
            << L'\n'
            << L"    ID: "
            << endpoint.id
            << L'\n';
    }
}

int RunBridge(
    const Endpoint& audioSink,
    const Endpoint& output)
{
    //
    // Capture side: AudioSink WASAPI loopback
    //

    ComPtr<IAudioClient> captureAudioClient;

    HRESULT hr = audioSink.device->Activate(
        __uuidof(IAudioClient),
        CLSCTX_ALL,
        nullptr,
        reinterpret_cast<void**>(
            captureAudioClient.GetAddressOf()));

    if (FAILED(hr))
    {
        PrintHresult("AudioSink Activate", hr);
        return 1;
    }

    WAVEFORMATEX* rawFormat = nullptr;

    hr = captureAudioClient->GetMixFormat(&rawFormat);

    if (FAILED(hr))
    {
        PrintHresult("AudioSink GetMixFormat", hr);
        return 1;
    }

    std::unique_ptr<WAVEFORMATEX, decltype(&CoTaskMemFree)>
        format(rawFormat, &CoTaskMemFree);

    std::wcout
        << L"Input format:\n"
        << L"    Sample rate: "
        << format->nSamplesPerSec
        << L" Hz\n"
        << L"    Channels: "
        << format->nChannels
        << L"\n"
        << L"    Bits/sample: "
        << format->wBitsPerSample
        << L"\n\n";

    // Smoke-test choice:
    //
    // Use a large-ish 100 ms shared buffer.
    //
    // TODO:
    // Replace this with event-driven low-latency buffering later.
    constexpr REFERENCE_TIME bufferDuration = 100 * 10'000;

    hr = captureAudioClient->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        AUDCLNT_STREAMFLAGS_LOOPBACK,
        bufferDuration,
        0,
        format.get(),
        nullptr);

    if (FAILED(hr))
    {
        PrintHresult("Loopback Initialize", hr);
        return 1;
    }

    ComPtr<IAudioCaptureClient> captureClient;

    hr = captureAudioClient->GetService(
        __uuidof(IAudioCaptureClient),
        reinterpret_cast<void**>(
            captureClient.GetAddressOf()));

    if (FAILED(hr))
    {
        PrintHresult("Get IAudioCaptureClient", hr);
        return 1;
    }

    //
    // Render side: selected physical endpoint
    //

    ComPtr<IAudioClient> renderAudioClient;

    hr = output.device->Activate(
        __uuidof(IAudioClient),
        CLSCTX_ALL,
        nullptr,
        reinterpret_cast<void**>(
            renderAudioClient.GetAddressOf()));

    if (FAILED(hr))
    {
        PrintHresult("Output Activate", hr);
        return 1;
    }

    //
    // Give the render stream exactly the same format that loopback
    // gives us.
    //
    // AUTOCONVERTPCM asks Windows to convert it to the physical
    // endpoint's mix format when necessary.
    //
    // TODO:
    // Later we will explicitly control our internal processing format.
    //
    constexpr DWORD renderFlags =
        AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM |
        AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY;

    hr = renderAudioClient->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        renderFlags,
        bufferDuration,
        0,
        format.get(),
        nullptr);

    if (FAILED(hr))
    {
        PrintHresult("Output Initialize", hr);
        return 1;
    }

    UINT32 renderBufferFrames = 0;

    hr = renderAudioClient->GetBufferSize(
        &renderBufferFrames);

    if (FAILED(hr))
    {
        PrintHresult("Output GetBufferSize", hr);
        return 1;
    }

    ComPtr<IAudioRenderClient> renderClient;

    hr = renderAudioClient->GetService(
        __uuidof(IAudioRenderClient),
        reinterpret_cast<void**>(
            renderClient.GetAddressOf()));

    if (FAILED(hr))
    {
        PrintHresult("Get IAudioRenderClient", hr);
        return 1;
    }

    //
    // Start both streams.
    //

    hr = renderAudioClient->Start();

    if (FAILED(hr))
    {
        PrintHresult("Output Start", hr);
        return 1;
    }

    hr = captureAudioClient->Start();

    if (FAILED(hr))
    {
        PrintHresult("Loopback Start", hr);
        renderAudioClient->Stop();
        return 1;
    }

    std::wcout
        << L"Forwarding:\n"
        << L"    "
        << audioSink.name
        << L"\n"
        << L"        -> "
        << output.name
        << L"\n\n"
        << L"Press Ctrl+C to stop.\n";

    uint64_t droppedPackets = 0;
    bool failed = false;

    while (g_running)
    {
        UINT32 packetFrames = 0;

        hr = captureClient->GetNextPacketSize(
            &packetFrames);

        if (FAILED(hr))
        {
            PrintHresult("GetNextPacketSize", hr);
            failed = true;
            break;
        }

        if (packetFrames == 0)
        {
            //
            // SMOKE TEST ONLY:
            //
            // Poll instead of waiting for a WASAPI event.
            //
            // TODO:
            // Replace this with event-driven buffering.
            //
            Sleep(1);
            continue;
        }

        //
        // Drain every currently available capture packet.
        //
        while (packetFrames > 0)
        {
            BYTE* captureData = nullptr;
            UINT32 frames = 0;
            DWORD captureFlags = 0;

            hr = captureClient->GetBuffer(
                &captureData,
                &frames,
                &captureFlags,
                nullptr,
                nullptr);

            if (FAILED(hr))
            {
                PrintHresult("Capture GetBuffer", hr);
                failed = true;
                break;
            }

            UINT32 padding = 0;

            hr = renderAudioClient->GetCurrentPadding(
                &padding);

            if (FAILED(hr))
            {
                PrintHresult("Output GetCurrentPadding", hr);

                captureClient->ReleaseBuffer(frames);
                failed = true;
                break;
            }

            const UINT32 availableFrames =
                renderBufferFrames - padding;

            if (frames <= availableFrames)
            {
                BYTE* renderData = nullptr;

                hr = renderClient->GetBuffer(
                    frames,
                    &renderData);

                if (FAILED(hr))
                {
                    PrintHresult("Output GetBuffer", hr);

                    captureClient->ReleaseBuffer(frames);
                    failed = true;
                    break;
                }

                DWORD outputFlags = 0;

                if (captureFlags & AUDCLNT_BUFFERFLAGS_SILENT)
                {
                    outputFlags = AUDCLNT_BUFFERFLAGS_SILENT;
                }
                else
                {
                    std::memcpy(
                        renderData,
                        captureData,
                        static_cast<size_t>(frames) *
                            format->nBlockAlign);
                }

                hr = renderClient->ReleaseBuffer(
                    frames,
                    outputFlags);

                if (FAILED(hr))
                {
                    PrintHresult("Output ReleaseBuffer", hr);

                    captureClient->ReleaseBuffer(frames);
                    failed = true;
                    break;
                }
            }
            else
            {
                //
                // SMOKE TEST ONLY:
                //
                // The output buffer cannot currently fit this packet.
                // Just throw the entire packet away.
                //
                // TODO:
                // Replace this with a FIFO/ring buffer.
                //
                ++droppedPackets;
            }

            hr = captureClient->ReleaseBuffer(frames);

            if (FAILED(hr))
            {
                PrintHresult("Capture ReleaseBuffer", hr);
                failed = true;
                break;
            }

            hr = captureClient->GetNextPacketSize(
                &packetFrames);

            if (FAILED(hr))
            {
                PrintHresult("GetNextPacketSize", hr);
                failed = true;
                break;
            }
        }

        if (failed)
            break;
    }

    captureAudioClient->Stop();
    renderAudioClient->Stop();

    std::wcout
        << L"\nStopped.\n"
        << L"Dropped packets: "
        << droppedPackets
        << L'\n';

    return failed ? 1 : 0;
}

int RunApp(int argc, wchar_t* argv[])
{
    ComPtr<IMMDeviceEnumerator> enumerator;

    HRESULT hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator),
        nullptr,
        CLSCTX_ALL,
        IID_PPV_ARGS(enumerator.GetAddressOf()));

    if (FAILED(hr))
    {
        PrintHresult("CoCreateInstance", hr);
        return 1;
    }

    std::vector<Endpoint> endpoints;

    hr = EnumerateRenderEndpoints(
        enumerator.Get(),
        endpoints);

    if (FAILED(hr))
    {
        PrintHresult("EnumAudioEndpoints", hr);
        return 1;
    }

    //
    // AudioSink is mandatory.
    //

    const auto audioSinkIt = std::find_if(
        endpoints.begin(),
        endpoints.end(),
        IsAudioSink);

    if (audioSinkIt == endpoints.end())
    {
        std::wcerr
            << L"ERROR: AudioSink render endpoint is not available.\n";

        return 1;
    }

    //
    // No output requested -> just list choices.
    //

    if (argc < 2)
    {
        PrintOutputEndpoints(endpoints);
        return 0;
    }

    const std::wstring_view filter = argv[1];

    //
    // First non-AudioSink endpoint containing the requested
    // substring wins.
    //

    const auto outputIt = std::find_if(
        endpoints.begin(),
        endpoints.end(),
        [&](const Endpoint& endpoint)
        {
            return
                !IsAudioSink(endpoint) &&
                ContainsInsensitive(
                    endpoint.name,
                    filter);
        });

    if (outputIt == endpoints.end())
    {
        std::wcerr
            << L"ERROR: No output endpoint matches \""
            << filter
            << L"\".\n\n";

        PrintOutputEndpoints(endpoints);
        return 1;
    }

    std::wcout
        << L"Selected output endpoint:\n"
        << L"    "
        << outputIt->name
        << L'\n'
        << L"    ID: "
        << outputIt->id
        << L"\n\n";

    return RunBridge(
        *audioSinkIt,
        *outputIt);
}

} // namespace

int wmain(int argc, wchar_t* argv[])
{
    HRESULT hr = CoInitializeEx(
        nullptr,
        COINIT_MULTITHREADED);

    if (FAILED(hr))
    {
        PrintHresult("CoInitializeEx", hr);
        return 1;
    }

    SetConsoleCtrlHandler(
        ConsoleHandler,
        TRUE);

    const int result = RunApp(
        argc,
        argv);

    CoUninitialize();

    return result;
}
