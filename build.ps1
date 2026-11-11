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
New-Item -ItemType Directory -Force build,build\v03,dist\TinyPTZ-v0.3 | Out-Null
& $compiler @common /c src/camera.cpp src/presets.cpp src/protocol.cpp src/settings.cpp /Fobuild\
if($LASTEXITCODE -ne 0){throw 'Core compilation failed'}
& "$vc\bin\Hostx64\x64\lib.exe" /nologo /OUT:build/ptz_core.lib build/camera.obj build/presets.obj build/protocol.obj build/settings.obj
if($LASTEXITCODE -ne 0){throw 'Core library build failed'}
$libs=@('build/ptz_core.lib','ole32.lib','oleaut32.lib','strmiids.lib','user32.lib')
& $compiler @common src/app.cpp src/settings_ui.cpp /Fobuild\ /Febuild/v03/TinyPTZ.exe @libs comctl32.lib comdlg32.lib shell32.lib gdi32.lib /link /SUBSYSTEM:WINDOWS
if($LASTEXITCODE -ne 0){throw 'Desktop compilation failed'}
& $compiler @common src/cli.cpp /Fobuild/cli.obj /Febuild/ptzctl.exe @libs
if($LASTEXITCODE -ne 0){throw 'CLI compilation failed'}
& $compiler @common tests/core_tests.cpp /Fobuild/tests.obj /Febuild/core_tests.exe @libs
if($LASTEXITCODE -ne 0){throw 'Test compilation failed'}
& .\build\core_tests.exe
if($LASTEXITCODE -ne 0){throw 'Core tests failed'}
& $compiler @common tests/settings_ui_tests.cpp /Fobuild/ui_tests.obj /Febuild/settings_ui_tests.exe @libs comctl32.lib gdi32.lib
if($LASTEXITCODE -ne 0){throw 'Settings UI test compilation failed'}
& .\build\settings_ui_tests.exe
if($LASTEXITCODE -ne 0){throw 'Settings UI regression failed'}
Copy-Item -LiteralPath build\v03\TinyPTZ.exe,build\ptzctl.exe,README.md -Destination dist\TinyPTZ-v0.3 -Force
Copy-Item -LiteralPath docs -Destination dist\TinyPTZ-v0.3 -Recurse -Force
New-Item -ItemType Directory -Force dist\TinyPTZ-v0.3\research | Out-Null
Copy-Item -LiteralPath research\NOTES.md,research\hardware-validation.txt,research\settings-validation-final.txt,research\boot-validation.txt,research\image-controls-validation.txt -Destination dist\TinyPTZ-v0.3\research -Force
$newPresets=Join-Path $PSScriptRoot 'dist\TinyPTZ-v0.3\data\presets.ptz'
if(-not (Test-Path -LiteralPath $newPresets)){
    foreach($oldPresets in @('dist\TinyPTZ-v0.2\data\presets.ptz','dist\TinyPTZ\data\presets.ptz','build\data\presets.ptz')){
        if(Test-Path -LiteralPath $oldPresets){
            New-Item -ItemType Directory -Force (Split-Path -Parent $newPresets) | Out-Null
            Copy-Item -LiteralPath $oldPresets -Destination $newPresets
            break
        }
    }
}
if($Package){Compress-Archive -LiteralPath dist\TinyPTZ-v0.3 -DestinationPath dist\TinyPTZ-v0.3-Windows-x64.zip -Force}
Write-Host 'Portable app: dist\TinyPTZ-v0.3\TinyPTZ.exe'
