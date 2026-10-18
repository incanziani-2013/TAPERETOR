; Inno Setup script for the TRAPERETOR Windows installer.
; Build: iscc /DAppVersion=1.0.0 /DBuildDir=<...>\TRAPERETOR_artefacts\Release /DOutputDir=<...> TRAPERETOR.iss
; The VST2 component is included only when BuildDir\VST\TRAPERETOR.dll exists.

#ifndef AppVersion
  #define AppVersion "1.0.0"
#endif
#ifndef BuildDir
  #define BuildDir "..\..\build\TRAPERETOR_artefacts\Release"
#endif
#ifndef OutputDir
  #define OutputDir "..\..\dist"
#endif
#if FileExists(BuildDir + "\VST\TRAPERETOR.dll")
  #define HasVST2
#endif

[Setup]
AppId={{D5A46A02-FB6E-42E0-B7A4-D3A9E138ABA5}
AppName=TRAPERETOR
AppVersion={#AppVersion}
AppVerName=TRAPERETOR {#AppVersion}
AppPublisher=TRAPERETOR
DefaultDirName={commonpf64}\TRAPERETOR
DisableDirPage=yes
DisableProgramGroupPage=yes
UninstallDisplayName=TRAPERETOR
LicenseFile=..\..\LICENSE
OutputDir={#OutputDir}
OutputBaseFilename=TRAPERETOR-{#AppVersion}-Windows-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible

[Types]
Name: "full"; Description: "Full installation"
Name: "custom"; Description: "Custom installation"; Flags: iscustom

[Components]
Name: "vst3"; Description: "VST3 plugin"; Types: full
#ifdef HasVST2
Name: "vst2"; Description: "VST2 plugin"; Types: full
#endif

[Files]
Source: "{#BuildDir}\VST3\TRAPERETOR.vst3\*"; DestDir: "{commoncf64}\VST3\TRAPERETOR.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
#ifdef HasVST2
Source: "{#BuildDir}\VST\TRAPERETOR.dll"; DestDir: "{code:GetVST2Dir}"; Components: vst2; Flags: ignoreversion
#endif

#ifdef HasVST2
[Code]
var
  VST2DirPage: TInputDirWizardPage;

procedure InitializeWizard;
begin
  VST2DirPage := CreateInputDirPage(wpSelectComponents,
    'VST2 plugin folder', 'Where should the VST2 plugin be installed?',
    'Pick the folder your DAW scans for VST2 plugins, then click Next.', False, '');
  VST2DirPage.Add('');
  VST2DirPage.Values[0] := GetPreviousData('VST2Dir', ExpandConstant('{commoncf64}\VST2'));
end;

function ShouldSkipPage(PageID: Integer): Boolean;
begin
  Result := (PageID = VST2DirPage.ID) and not WizardIsComponentSelected('vst2');
end;

procedure RegisterPreviousData(PreviousDataKey: Integer);
begin
  SetPreviousData(PreviousDataKey, 'VST2Dir', VST2DirPage.Values[0]);
end;

function GetVST2Dir(Param: String): String;
begin
  Result := VST2DirPage.Values[0];
end;
#endif
