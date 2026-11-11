param([switch]$Package)
$ErrorActionPreference='Stop'
Set-Location $PSScriptRoot
$vswhere='C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe'
$vs=& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(-not $vs){throw 'Visual Studio C++ Build Tools are required.'}
$vc=Get-ChildItem -LiteralPath "$vs\VC\Tools\MSVC" -Directory | Sort-Object Name -Descending | Select-Object -First 1 -ExpandProperty FullName
$kit='C:\Program Files (x86)\Windows Kits\10'
$version=Get-ChildItem -LiteralPath "$kit\Include" -Directory | Where-Object {Test-Path -LiteralPath "$($_.FullName)\um\windows.h"} | Sort-Object Name -Descending | Select-Object -First 1 -ExpandProperty Name
$env:INCLUDE="$vc\include;$kit\Include\$version\ucrt;$kit\Include\$version\shared;$kit\Include\$version\um;$kit\Include\$version\winrt"
$env:LIB="$vc\lib\x64;$kit\Lib\$version\ucrt\x64;$kit\Lib\$version\um\x64"
$compiler="$vc\bin\Hostx64\x64\cl.exe"
$common=@('/nologo','/EHsc','/std:c++17','/MT','/O2','/W4','/utf-8','/DUNICODE','/D_UNICODE','/DNOMINMAX','/Isrc')
New-Item -ItemType Directory -Force build,dist\TinyPTZ | Out-Null
& $compiler @common /c src/camera.cpp src/presets.cpp src/protocol.cpp /Fobuild\
if($LASTEXITCODE -ne 0){throw 'Core compilation failed'}
& "$vc\bin\Hostx64\x64\lib.exe" /nologo /OUT:build/ptz_core.lib build/camera.obj build/presets.obj build/protocol.obj
if($LASTEXITCODE -ne 0){throw 'Core library build failed'}
$libs=@('build/ptz_core.lib','ole32.lib','oleaut32.lib','strmiids.lib','user32.lib')
& $compiler @common src/app.cpp /Fobuild/app.obj /Febuild/TinyPTZ.exe @libs comctl32.lib comdlg32.lib shell32.lib gdi32.lib /link /SUBSYSTEM:WINDOWS
if($LASTEXITCODE -ne 0){throw 'Desktop compilation failed'}
& $compiler @common src/cli.cpp /Fobuild/cli.obj /Febuild/ptzctl.exe @libs
if($LASTEXITCODE -ne 0){throw 'CLI compilation failed'}
& $compiler @common tests/core_tests.cpp /Fobuild/tests.obj /Febuild/core_tests.exe @libs
if($LASTEXITCODE -ne 0){throw 'Test compilation failed'}
& .\build\core_tests.exe
if($LASTEXITCODE -ne 0){throw 'Core tests failed'}
Copy-Item -LiteralPath build\TinyPTZ.exe,build\ptzctl.exe,README.md -Destination dist\TinyPTZ -Force
Copy-Item -LiteralPath docs -Destination dist\TinyPTZ -Recurse -Force
New-Item -ItemType Directory -Force dist\TinyPTZ\research | Out-Null
Copy-Item -LiteralPath research\NOTES.md,research\hardware-validation.txt -Destination dist\TinyPTZ\research -Force
if($Package){Compress-Archive -LiteralPath dist\TinyPTZ -DestinationPath dist\TinyPTZ-Windows-x64.zip -Force}
Write-Host 'Portable app: dist\TinyPTZ\TinyPTZ.exe'
