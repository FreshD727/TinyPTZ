param([switch]$Package, [switch]$Setup, [switch]$HardwareTest, [string]$QtRoot = '')
$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot
if (-not $QtRoot) { $QtRoot = Join-Path $PSScriptRoot 'build\obs-sdk\qt6' }
if ($Setup -and -not (Test-Path -LiteralPath "$QtRoot\include\QtWidgets\QWidget")) {
    $sdk = Join-Path $PSScriptRoot 'build\obs-sdk'; New-Item -ItemType Directory -Force $sdk | Out-Null
    $archive = Join-Path $sdk 'qt6.zip'
    if (-not (Test-Path -LiteralPath $archive)) {
        & curl.exe -sS -L --fail --retry 2 -o $archive 'https://github.com/obsproject/obs-deps/releases/download/2024-09-12/windows-deps-qt6-2024-09-12-x64.zip'
        if ($LASTEXITCODE -ne 0) { throw 'Qt download failed.' }
    }
    if ((Get-FileHash -LiteralPath $archive).Hash -ne '4D15CE13DBB0A8A2CABCCE5AE0DA5E80EE589B482A61B2025378465C1DA32C4F') { throw 'Qt checksum mismatch.' }
    Expand-Archive -LiteralPath $archive -DestinationPath $QtRoot -Force
}
if (-not (Test-Path -LiteralPath "$QtRoot\include\QtWidgets\QWidget")) { throw 'Run .\build-desktop.ps1 -Setup to download Qt, or pass -QtRoot.' }
$vs = & 'C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe' -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw 'Visual Studio C++ Build Tools are required.' }
$vc = Get-ChildItem -LiteralPath "$vs\VC\Tools\MSVC" -Directory | Sort-Object Name -Descending | Select-Object -First 1 -ExpandProperty FullName
$kit = 'C:\Program Files (x86)\Windows Kits\10'
$version = Get-ChildItem -LiteralPath "$kit\Include" -Directory | Where-Object { Test-Path -LiteralPath "$($_.FullName)\um\windows.h" } | Sort-Object Name -Descending | Select-Object -First 1 -ExpandProperty Name
$env:INCLUDE = "$vc\include;$kit\Include\$version\ucrt;$kit\Include\$version\shared;$kit\Include\$version\um;$kit\Include\$version\winrt"
$env:LIB = "$vc\lib\x64;$kit\Lib\$version\ucrt\x64;$kit\Lib\$version\um\x64;$QtRoot\lib"
$out = Join-Path $PSScriptRoot 'build\desktop'; New-Item -ItemType Directory -Force $out | Out-Null
$compiler = "$vc\bin\Hostx64\x64\cl.exe"
$common = @('/nologo','/EHsc','/std:c++17','/MD','/O2','/W4','/utf-8','/Zc:__cplusplus','/permissive-','/DUNICODE','/D_UNICODE','/DNOMINMAX','/DQT_NO_KEYWORDS','/Isrc','/Iobs',"/I$QtRoot\include","/I$QtRoot\include\QtCore","/I$QtRoot\include\QtGui","/I$QtRoot\include\QtWidgets")
& $compiler @common /c src/camera.cpp src/presets.cpp src/protocol.cpp src/settings.cpp obs/camera_worker.cpp obs/dock.cpp src/qt_window.cpp "/Fo$out\"
if ($LASTEXITCODE -ne 0) { throw 'Qt desktop compilation failed.' }
$core = @('camera','presets','protocol','settings') | ForEach-Object { "$out\$_.obj" }
$controls = @("$out\camera_worker.obj", "$out\dock.obj")
$native = @('ole32.lib','oleaut32.lib','strmiids.lib','user32.lib')
$qt = @('Qt6Core.lib','Qt6Gui.lib','Qt6Widgets.lib','dwmapi.lib')
& $compiler @common src/qt_app.cpp "/Fo$out\qt_app.obj" "/Fe$out\TinyPTZ.exe" @core @controls "$out\qt_window.obj" @native @qt Qt6EntryPoint.lib shell32.lib /link /SUBSYSTEM:WINDOWS
if ($LASTEXITCODE -ne 0) { throw 'Qt desktop link failed.' }
& $compiler @common src/cli.cpp "/Fo$out\cli.obj" "/Fe$out\ptzctl.exe" @core @native
if ($LASTEXITCODE -ne 0) { throw 'CLI build failed.' }
& $compiler @common tests/core_tests.cpp "/Fo$out\core_tests.obj" "/Fe$out\core_tests.exe" @core @native
if ($LASTEXITCODE -ne 0) { throw 'Core test build failed.' }
& $compiler @common tests/obs_tests.cpp "/Fo$out\controls_tests.obj" "/Fe$out\controls_tests.exe" @core @controls @native @qt
if ($LASTEXITCODE -ne 0) { throw 'Shared controls test build failed.' }
& $compiler @common tests/desktop_tests.cpp "/Fo$out\desktop_tests.obj" "/Fe$out\desktop_tests.exe" @core @controls "$out\qt_window.obj" @native @qt
if ($LASTEXITCODE -ne 0) { throw 'Desktop test build failed.' }
$oldPath = $env:PATH; $oldPlatform = $env:QT_QPA_PLATFORM; $oldPluginPath = $env:QT_QPA_PLATFORM_PLUGIN_PATH
try {
    $env:PATH = "$QtRoot\bin;$env:PATH"; $env:QT_QPA_PLATFORM = 'minimal'; $env:QT_QPA_PLATFORM_PLUGIN_PATH = "$QtRoot\plugins\platforms"
    foreach ($test in @('core_tests','controls_tests','desktop_tests')) {
        & "$out\$test.exe"
        if ($LASTEXITCODE -ne 0) { throw "$test failed." }
    }
    if ($HardwareTest) {
        $env:QT_QPA_PLATFORM = 'windows'
        & "$out\desktop_tests.exe" --hardware
        if ($LASTEXITCODE -ne 0) { throw 'Desktop hardware test failed.' }
    }
} finally { $env:PATH = $oldPath; $env:QT_QPA_PLATFORM = $oldPlatform; $env:QT_QPA_PLATFORM_PLUGIN_PATH = $oldPluginPath }
if ($Package) {
    $release = Join-Path $PSScriptRoot 'dist\TinyPTZ-v0.5.0'
    New-Item -ItemType Directory -Force $release,"$release\platforms","$release\sources","$release\licenses\Qt","$release\data" | Out-Null
    Copy-Item -LiteralPath "$out\TinyPTZ.exe","$out\ptzctl.exe",README.md,THIRD-PARTY-NOTICES.md -Destination $release -Force
    Copy-Item -LiteralPath "$QtRoot\bin\Qt6Core.dll","$QtRoot\bin\Qt6Gui.dll","$QtRoot\bin\Qt6Widgets.dll" -Destination $release -Force
    Copy-Item -LiteralPath "$QtRoot\plugins\platforms\qwindows.dll" -Destination "$release\platforms" -Force
    $redist = Get-ChildItem -LiteralPath "$vs\VC\Redist\MSVC" -Directory | Where-Object { $_.Name -match '^\d' } | Sort-Object Name -Descending | Select-Object -First 1 -ExpandProperty FullName
    Copy-Item -Path "$redist\x64\Microsoft.VC145.CRT\*.dll" -Destination $release -Force
    Copy-Item -LiteralPath docs -Destination $release -Recurse -Force
    if (-not (Test-Path -LiteralPath "$release\data\presets.ptz")) {
        foreach ($candidate in @('dist\TinyPTZ-v0.3.4\data\presets.ptz','build\data\presets.ptz')) {
            if (Test-Path -LiteralPath $candidate) { Copy-Item -LiteralPath $candidate -Destination "$release\data\presets.ptz"; break }
        }
    }
    $source = Join-Path $PSScriptRoot 'build\desktop-deps\qtbase-everywhere-src-6.6.3.tar.xz'
    if (-not (Test-Path -LiteralPath $source)) {
        New-Item -ItemType Directory -Force (Split-Path -Parent $source) | Out-Null
        & curl.exe -sS -L --fail --retry 2 -o $source 'https://download.qt.io/archive/qt/6.6/6.6.3/submodules/qtbase-everywhere-src-6.6.3.tar.xz'
        if ($LASTEXITCODE -ne 0) { throw 'Qt source download failed.' }
    }
    if ((Get-FileHash -LiteralPath $source).Hash -ne '0493FD0B380C4EDF8872F011A7F26D245AA4CDD75B349904EF340A22DEDF7462') { throw 'Qt source checksum mismatch.' }
    Copy-Item -LiteralPath $source -Destination "$release\sources" -Force
    $licenseTemp = Join-Path $PSScriptRoot 'build\desktop-deps\notices'; New-Item -ItemType Directory -Force $licenseTemp | Out-Null
    & tar.exe -xf $source -C $licenseTemp qtbase-everywhere-src-6.6.3/LICENSES
    if ($LASTEXITCODE -ne 0) { throw 'Qt license extraction failed.' }
    Copy-Item -Path "$licenseTemp\qtbase-everywhere-src-6.6.3\LICENSES\*" -Destination "$release\licenses\Qt" -Force
    Copy-Item -LiteralPath "$licenseTemp\qtbase-everywhere-src-6.6.3\LICENSES\GPL-2.0-only.txt" -Destination "$release\LICENSE.txt" -Force
    Compress-Archive -LiteralPath src,obs,tests,CMakeLists.txt,build.ps1,build-desktop.ps1,README.md,THIRD-PARTY-NOTICES.md,docs -DestinationPath "$release\sources\TinyPTZ-source.zip" -Force
    if (Test-Path -LiteralPath research/desktop-qt-validation.txt) { Copy-Item -LiteralPath research/desktop-qt-validation.txt -Destination "$release\VALIDATION.txt" -Force }
    Compress-Archive -LiteralPath $release -DestinationPath dist/TinyPTZ-v0.5.0-Windows-x64.zip -Force
    Write-Host "Portable Qt app: $release\TinyPTZ.exe"
}
