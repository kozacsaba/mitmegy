#define MyAppName "WormHole"
#define MyAppVersion "0.1.0"

[Setup]
AppId={{31E286C0-43EF-4B70-84EA-2239DF633580}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
OutputDir=..\dist
OutputBaseFilename=WormHole-Setup-{#MyAppVersion}-x64
Compression=lzma2
SolidCompression=yes
PrivilegesRequired=admin
ArchitecturesAllowed=x64os
ArchitecturesInstallIn64BitMode=x64os
MinVersion=10.0.19041
WizardStyle=modern
UninstallDisplayIcon={app}\controller.exe
SetupLogging=yes
CloseApplications=yes

[Files]
Source: "..\build\vs2026-x64\src\processor\Release\processor.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\build\vs2026-x64\src\controller\controller_artefacts\Release\controller.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\vendor\VBCABLE_Driver_Pack46.zip"; DestDir: "{app}\vendor"; Flags: ignoreversion
Source: "task.ps1"; DestDir: "{app}\installer"; Flags: ignoreversion
Source: "vbcable.ps1"; DestDir: "{app}\installer"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\{#MyAppName}"; Filename: "{app}\controller.exe"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\controller.exe"; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Additional shortcuts:"; Flags: unchecked

[Code]
var
  VBCableNoticePage: TWizardPage;
  VBLink: TNewStaticText;
  DriverRebootRequired: Boolean;
  RemoveVBCableAfterUninstall: Boolean;
  UninstallDriverSucceeded: Boolean;

function PsPath: String;
begin
  Result := ExpandConstant('{sys}\WindowsPowerShell\v1.0\powershell.exe');
end;

procedure VBLinkClick(Sender: TObject);
var
  ErrorCode: Integer;
begin
  ShellExec('open', 'https://vb-audio.com/Cable/', '', '', SW_SHOWNORMAL, ewNoWait, ErrorCode);
end;

procedure InitializeWizard;
var
  Description: TNewStaticText;
begin
  VBCableNoticePage := CreateCustomPage(wpWelcome, 'VB-CABLE audio driver', 'Third-party component notice');

  Description := TNewStaticText.Create(WizardForm);
  Description.Parent := VBCableNoticePage.Surface;
  Description.Left := ScaleX(0);
  Description.Top := ScaleY(8);
  Description.Width := VBCableNoticePage.SurfaceWidth;
  Description.Height := ScaleY(105);
  Description.AutoSize := False;
  Description.WordWrap := True;
  Description.Caption :=
    'This application uses VB-CABLE, a virtual audio driver developed and published by VB-Audio Software.' + #13#10 + #13#10 +
    'VB-CABLE is donationware. If it is useful to you, VB-Audio welcomes contributions. ' +
    'Setup will preserve an existing VB-CABLE installation. A restart is required when installing a new copy.';

  VBLink := TNewStaticText.Create(WizardForm);
  VBLink.Parent := VBCableNoticePage.Surface;
  VBLink.Left := ScaleX(0);
  VBLink.Top := ScaleY(125);
  VBLink.Caption := 'VB-CABLE website and donations';
  VBLink.Font.Color := clBlue;
  VBLink.Font.Style := [fsUnderline];
  VBLink.Cursor := crHand;
  VBLink.OnClick := @VBLinkClick;
end;

procedure CurStepChanged(CurStep: TSetupStep);
var
  ExitCode: Integer;
  Command: String;
begin
  if CurStep <> ssPostInstall then Exit;

  Command := '-NoProfile -NonInteractive -ExecutionPolicy Bypass -File "' +
    ExpandConstant('{app}\installer\vbcable.ps1') + '" -Mode Install -ZipFile "' +
    ExpandConstant('{app}\vendor\VBCABLE_Driver_Pack46.zip') + '" -OwnerMarker "' +
    ExpandConstant('{app}\vbcable-owned.flag') + '" -RestartMarker "' +
    ExpandConstant('{app}\vbcable-restart.flag') + '"';

  if not Exec(PsPath, Command, '', SW_HIDE, ewWaitUntilTerminated, ExitCode) or (ExitCode <> 0) then
    RaiseException('VB-CABLE installation failed. Check the Setup log.');

  DriverRebootRequired := FileExists(ExpandConstant('{app}\vbcable-restart.flag'));

  Command := '-NoProfile -NonInteractive -ExecutionPolicy Bypass -File "' +
    ExpandConstant('{app}\installer\task.ps1') + '" -Mode Install -Processor "' +
    ExpandConstant('{app}\processor.exe') + '"';

  if not ExecAsOriginalUser(PsPath, Command, '', SW_HIDE, ewWaitUntilTerminated, ExitCode) or (ExitCode <> 0) then
    RaiseException('Could not register the per-user background task. Start Setup normally (not via Run as administrator) and see the log.');
end;

function NeedRestart: Boolean;
begin
  Result := DriverRebootRequired;
end;

function InitializeUninstall: Boolean;
begin
  Result := True;
  RemoveVBCableAfterUninstall := False;
  UninstallDriverSucceeded := False;
  if FileExists(ExpandConstant('{app}\vbcable-owned.flag')) then
    RemoveVBCableAfterUninstall :=
      MsgBox('This installer originally installed VB-CABLE.' + #13#10 +
        'Remove VB-CABLE too? Other applications may now depend on it.' + #13#10 +
        'Select No to keep the shared driver installed.', mbConfirmation, MB_YESNO) = IDYES;
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  ExitCode: Integer;
  Command: String;
begin
  if CurUninstallStep <> usUninstall then Exit;

  Command := '-NoProfile -NonInteractive -ExecutionPolicy Bypass -File "' +
    ExpandConstant('{app}\installer\task.ps1') + '" -Mode Uninstall';
  if not Exec(PsPath, Command, '', SW_HIDE, ewWaitUntilTerminated, ExitCode) or (ExitCode <> 0) then
    MsgBox('The scheduled task could not be removed automatically. Remove "WormHole Processor" in Task Scheduler.', mbError, MB_OK);

  if RemoveVBCableAfterUninstall then begin
    Command := '-NoProfile -NonInteractive -ExecutionPolicy Bypass -File "' +
      ExpandConstant('{app}\installer\vbcable.ps1') + '" -Mode Uninstall -ZipFile "' +
      ExpandConstant('{app}\vendor\VBCABLE_Driver_Pack46.zip') + '" -OwnerMarker "' +
      ExpandConstant('{app}\vbcable-owned.flag') + '"';
    UninstallDriverSucceeded := Exec(PsPath, Command, '', SW_HIDE, ewWaitUntilTerminated, ExitCode) and (ExitCode = 0);
    if not UninstallDriverSucceeded then
      MsgBox('VB-CABLE removal failed. Use the official VB-CABLE setup utility to remove it manually.', mbError, MB_OK);
  end;
end;

function UninstallNeedRestart: Boolean;
begin
  Result := UninstallDriverSucceeded;
end;
