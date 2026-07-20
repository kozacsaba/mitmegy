# FxSound Driver — Build and Test Setup

## Summary

The original FxSound driver was updated to build with:

- Visual Studio Community 2026
- Windows SDK `10.0.28000.0`
- WDK `10.0.28000.0`

Required project changes:

- Retarget SDK from `10.0.22621.0` to `10.0.28000.0`.
- Update the INF to use Driver Store destination `DIRID 13`.
- Restrict the INF to Windows 10 build 16299 or newer.
- Suppress WDK warning `C4996`.
- Add `/fd SHA256` to driver and package signing.

Final package:

```text
fxvad\x64\Debug\package\
├── fxvad.inf
├── fxvad.sys
└── fxvadntamd64.cat
```

## Installed outside Visual Studio Installer

```cmd
winget install --id Microsoft.WindowsSDK.10.0.28000 --exact
winget install --id Microsoft.WindowsWDK.10.0.28000 --exact
```

Installed paths:

```text
Visual Studio:
G:\Program Files\Microsoft Visual Studio\18\Community\

Windows Kits:
G:\Windows Kits\10\
```

The WDK Visual Studio integration was installed separately through Visual Studio Installer.

## Developer environment

Create `activate.cmd` in the repository root:

```bat
@echo off

call "G:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" ^
    -arch=x64 ^
    -host_arch=x64

if errorlevel 1 (
    echo Failed to initialize the Visual Studio developer environment.
    exit /b 1
)

echo Visual Studio x64 developer environment activated.
```

Activate it from `cmd.exe`:

```cmd
call activate.cmd
```

## Build

From the repository root:

```cmd
msbuild fxvad\fxvad.sln /m /t:Build /p:Configuration=Debug /p:Platform=x64 /p:WindowsTargetPlatformVersion=10.0.28000.0
```

## First-time test setup

Run from an Administrator Command Prompt.

Enable test-signing:

```cmd
bcdedit /set testsigning on
```

Reboot afterward.

Trust the generated certificate:

```cmd
certutil -addstore -f Root ^
    "G:\Projects\fxsound-driver\fxvad\pcmex\x64\Debug\fxvad.cer"

certutil -addstore -f TrustedPublisher ^
    "G:\Projects\fxsound-driver\fxvad\pcmex\x64\Debug\fxvad.cer"
```

Install the virtual device once:

```cmd
"G:\Windows Kits\10\Tools\10.0.28000.0\x64\devcon.exe" install ^
    "G:\Projects\fxsound-driver\fxvad\x64\Debug\package\fxvad.inf" ^
    ROOT\FXVAD
```

Do not repeat `devcon install`, because it can create duplicate devices.

## Updating after rebuilds

Use `update` instead:

```cmd
"G:\Windows Kits\10\Tools\10.0.28000.0\x64\devcon.exe" update ^
    "G:\Projects\fxsound-driver\fxvad\x64\Debug\package\fxvad.inf" ^
    ROOT\FXVAD
```

Normal development flow:

```text
activate environment
→ build
→ devcon update
→ test
```
