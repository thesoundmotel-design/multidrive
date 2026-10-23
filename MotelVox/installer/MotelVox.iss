; Inno Setup script - installs the VST3 for all users
[Setup]
AppName=Motel Vox
AppVersion=1.0.0
AppPublisher=The Sound Motel
DefaultDirName={commoncf64}\VST3
DisableDirPage=yes
DisableProgramGroupPage=yes
OutputBaseFilename=MotelVox-Setup-Windows
ArchitecturesInstallIn64BitMode=x64compatible
Compression=lzma
PrivilegesRequired=admin

[Files]
Source: "..\build\MotelVox_artefacts\Release\VST3\Motel Vox.vst3\*"; DestDir: "{commoncf64}\VST3\Motel Vox.vst3"; Flags: recursesubdirs ignoreversion
