#include <windows.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>
#include <wrl/client.h>

#include <iostream>

using Microsoft::WRL::ComPtr;

int main()
{
  HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  if (FAILED(hr))
  {
    std::cerr << "CoInitializeEx failed: 0x"
      << std::hex << hr << '\n';
    return 1;
  }

  ComPtr<IMMDeviceEnumerator> enumerator;

  hr = CoCreateInstance(
    __uuidof(MMDeviceEnumerator),
    nullptr,
    CLSCTX_ALL,
    IID_PPV_ARGS(&enumerator));

  if (FAILED(hr))
  {
    std::cerr << "Failed to create MMDeviceEnumerator\n";
    CoUninitialize();
    return 1;
  }

  ComPtr<IMMDeviceCollection> devices;

  hr = enumerator->EnumAudioEndpoints(
    eRender,
    DEVICE_STATE_ACTIVE,
    &devices);

  if (FAILED(hr))
  {
    std::cerr << "EnumAudioEndpoints failed\n";
    CoUninitialize();
    return 1;
  }

  UINT count = 0;
  devices->GetCount(&count);

  std::wcout << L"Available render endpoints:\n\n";

  for (UINT i = 0; i < count; ++i)
  {
    ComPtr<IMMDevice> device;
    if (FAILED(devices->Item(i, &device)))
      continue;

    ComPtr<IPropertyStore> properties;
    if (FAILED(device->OpenPropertyStore(STGM_READ, &properties)))
      continue;

    PROPVARIANT name;
    PropVariantInit(&name);

    if (SUCCEEDED(properties->GetValue(PKEY_Device_FriendlyName, &name)))
    {
      if (name.vt == VT_LPWSTR)
        std::wcout << L"[" << i << L"] " << name.pwszVal << L'\n';
    }

    LPWSTR id = nullptr;

    if (SUCCEEDED(device->GetId(&id)))
    {
      std::wcout << L"    ID: " << id << L'\n';
      CoTaskMemFree(id);
    }

    PropVariantClear(&name);
    }

  CoUninitialize();
  return 0;
}
