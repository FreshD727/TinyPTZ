param(
    [switch]$Package,
    [switch]$Setup,
    [switch]$HardwareTest,
    [string]$ObsRoot = 'C:\Program Files\obs-studio'
)
$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot
$sdk = Join-Path $PSScriptRoot 'build\obs-sdk'
$source = Join-Path $sdk 'source\obs-studio-31.0.0-beta3'
$qt = Join-Path $sdk 'qt6'
if ($Setup) {
    New-Item -ItemType Directory -Force $sdk | Out-Null
    $downloads = @(
        @{File='qt6.zip'; Url='https://github.com/obsproject/obs-deps/releases/download/2024-09-12/windows-deps-qt6-2024-09-12-x64.zip'; Hash='4d15ce13dbb0a8a2cabcce5ae0da5e80ee589b482a61b2025378465c1da32c4f'; Folder='qt6'},
        @{File='obs-source.zip'; Url='https://github.com/obsproject/obs-studio/archive/refs/tags/31.0.0-beta3.zip'; Hash='c807ed7820a17971c5b2e4b449fe6bf162f5db218fef3d2aca3554bd6c4307a4'; Folder='source'}
    )
    foreach ($download in $downloads) {
        $archive = Join-Path $sdk $download.File
        if (-not (Test-Path -LiteralPath $archive)) {
            & curl.exe -sS -L --fail --retry 2 -o $archive $download.Url
            if ($LASTEXITCODE -ne 0) { throw "Download failed: $($download.File)" }
        }
        if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash -ne $download.Hash) { throw "Dependency checksum mismatch: $archive" }
        $folder = Join-Path $sdk $download.Folder
        if (-not (Test-Path -LiteralPath $folder)) { Expand-Archive -LiteralPath $archive -DestinationPath $folder }
    }
}
if (-not (Test-Path -LiteralPath "$qt\include\QtWidgets\QWidget")) { throw 'Run .\build-obs.ps1 -Setup once to download the official matching OBS/Qt development files.' }
$obsBin = Join-Path $ObsRoot 'bin\64bit'
if (-not (Test-Path -LiteralPath "$obsBin\obs.dll")) { throw 'Pass -ObsRoot pointing at an OBS Studio 31.x Windows x64 installation.' }
$qtVersion = (Get-Item -LiteralPath "$obsBin\Qt6Core.dll").VersionInfo.ProductVersion
$obsVersion = (Get-Item -LiteralPath "$obsBin\obs64.exe").VersionInfo.ProductVersion
if ($obsVersion -notlike '31.*') { throw "This SDK build targets OBS 31.x. Installed OBS: $obsVersion." }
if ($qtVersion -notlike '6.6.3*') { throw "This build targets Qt 6.6.3 (OBS 31). Installed Qt: $qtVersion. Use matching development files before building for other OBS versions." }
$vswhere = 'C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe'
$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw 'Visual Studio C++ Build Tools are required.' }
$vc = Get-ChildItem -LiteralPath "$vs\VC\Tools\MSVC" -Directory | Sort-Object Name -Descending | Select-Object -First 1 -ExpandProperty FullName
$kit = 'C:\Program Files (x86)\Windows Kits\10'
$version = Get-ChildItem -LiteralPath "$kit\Include" -Directory | Where-Object { Test-Path -LiteralPath "$($_.FullName)\um\windows.h" } | Sort-Object Name -Descending | Select-Object -First 1 -ExpandProperty Name
$env:INCLUDE = "$vc\include;$kit\Include\$version\ucrt;$kit\Include\$version\shared;$kit\Include\$version\um;$kit\Include\$version\winrt"
$env:LIB = "$vc\lib\x64;$kit\Lib\$version\ucrt\x64;$kit\Lib\$version\um\x64;$qt\lib"
$out = Join-Path $PSScriptRoot 'build\obs'
New-Item -ItemType Directory -Force $out | Out-Null
$bin = "$vc\bin\Hostx64\x64"
# Import libraries contain only the installed runtime's public exported symbols.
foreach ($dll in @('obs', 'obs-frontend-api')) {
    $exports = & "$bin\dumpbin.exe" /nologo /exports "$obsBin\$dll.dll"
    if ($LASTEXITCODE -ne 0) { throw "Cannot read OBS exports: $dll" }
    $symbols = @($exports | ForEach-Object { if ($_ -match '^\s+\d+\s+[0-9A-F]+\s+[0-9A-F]+\s+(\S+)') { $Matches[1] } })
    if ($symbols.Count -lt 50) { throw "Unexpected OBS export table: $dll" }
    @("LIBRARY $dll.dll", 'EXPORTS') + $symbols | Set-Content -LiteralPath "$out\$dll.def" -Encoding ascii
    & "$bin\lib.exe" /nologo /machine:x64 "/def:$out\$dll.def" "/out:$out\$dll.lib"
    if ($LASTEXITCODE -ne 0) { throw "Cannot generate import library: $dll" }
}
$common = @('/nologo', '/EHsc', '/std:c++17', '/MD', '/O2', '/W4', '/utf-8', '/Zc:__cplusplus', '/permissive-', '/DUNICODE', '/D_UNICODE', '/DNOMINMAX', '/DQT_NO_KEYWORDS', '/Isrc', '/Iobs', "/I$qt\include", "/I$qt\include\QtCore", "/I$qt\include\QtGui", "/I$qt\include\QtWidgets", "/I$source\libobs", "/I$source\UI\obs-frontend-api")
$core = @('src/camera.cpp', 'src/presets.cpp', 'src/protocol.cpp', 'src/settings.cpp')
& "$bin\cl.exe" @common /c @core obs/camera_worker.cpp obs/dock.cpp obs/plugin.cpp "/Fo$out\"
if ($LASTEXITCODE -ne 0) { throw 'OBS plugin compilation failed.' }
$objects = @('camera', 'presets', 'protocol', 'settings', 'camera_worker', 'dock') | ForEach-Object { "$out\$_.obj" }
$libs = @('ole32.lib', 'oleaut32.lib', 'strmiids.lib', 'user32.lib', 'Qt6Core.lib', 'Qt6Gui.lib', 'Qt6Widgets.lib')
& "$bin\link.exe" /nologo /DLL "/OUT:$out\tiny-ptz.dll" @objects "$out\plugin.obj" "$out\obs.lib" "$out\obs-frontend-api.lib" @libs
if ($LASTEXITCODE -ne 0) { throw 'OBS plugin linking failed.' }
& "$bin\cl.exe" @common tests/obs_tests.cpp "/Fo$out\obs_tests.obj" "/Fe$out\obs_tests.exe" @objects @libs
if ($LASTEXITCODE -ne 0) { throw 'OBS test compilation failed.' }
$hostOut = Join-Path $out 'module-host'
New-Item -ItemType Directory -Force $hostOut | Out-Null
& "$bin\cl.exe" @common /LD tests/obs_frontend_stub.cpp "/Fo$hostOut\frontend-stub.obj" "/Fe$hostOut\obs-frontend-api.dll" "$out\obs.lib" @libs /link "/IMPLIB:$hostOut\frontend-stub.lib"
if ($LASTEXITCODE -ne 0) { throw 'Isolated frontend fixture compilation failed.' }
& "$bin\cl.exe" @common tests/obs_module_tests.cpp "/Fo$hostOut\module-tests.obj" "/Fe$hostOut\module-tests.exe" "$hostOut\frontend-stub.lib" "$out\obs.lib" @objects @libs
if ($LASTEXITCODE -ne 0) { throw 'OBS module test compilation failed.' }
$previousPath = $env:PATH
$previousPlatform = $env:QT_QPA_PLATFORM
$previousPluginPath = $env:QT_QPA_PLATFORM_PLUGIN_PATH
$previousLogging = $env:QT_FORCE_STDERR_LOGGING
try {
    $env:PATH = "$qt\bin;$env:PATH"
    $env:QT_QPA_PLATFORM = 'minimal'
    $env:QT_QPA_PLATFORM_PLUGIN_PATH = "$qt\plugins\platforms"
    $env:QT_FORCE_STDERR_LOGGING = '1'
    & "$out\obs_tests.exe"
    if ($LASTEXITCODE -ne 0) { throw 'OBS dock tests failed.' }
    $env:PATH = "$obsBin;$qt\bin;$previousPath"
    & "$hostOut\module-tests.exe" "$out\tiny-ptz.dll"
    if ($LASTEXITCODE -ne 0) { throw 'OBS module loader/lifecycle tests failed.' }
    if ($HardwareTest) {
        & "$hostOut\module-tests.exe" "$out\tiny-ptz.dll" --hardware
        if ($LASTEXITCODE -ne 0) { throw 'OBS plugin hardware test failed.' }
    }
} finally {
    $env:PATH = $previousPath; $env:QT_QPA_PLATFORM = $previousPlatform
    $env:QT_QPA_PLATFORM_PLUGIN_PATH = $previousPluginPath; $env:QT_FORCE_STDERR_LOGGING = $previousLogging
}
if ($Package) {
    $release = Join-Path $PSScriptRoot 'dist\TinyPTZ-OBS-v0.4.0'
    New-Item -ItemType Directory -Force "$release\obs-plugins\64bit", "$release\data\obs-plugins\tiny-ptz" | Out-Null
    Copy-Item -LiteralPath "$out\tiny-ptz.dll" -Destination "$release\obs-plugins\64bit" -Force
    Copy-Item -LiteralPath docs/OBS-PLUGIN.md -Destination "$release\README.md" -Force
    Copy-Item -LiteralPath docs/SETTINGS.md -Destination "$release\SETTINGS.md" -Force
    if (Test-Path -LiteralPath research/obs-plugin-validation.txt) {
        Copy-Item -LiteralPath research/obs-plugin-validation.txt -Destination "$release\VALIDATION.txt" -Force
    }
    Copy-Item -LiteralPath obs/install-obs.ps1 -Destination $release -Force
    Copy-Item -LiteralPath "$source\COPYING" -Destination "$release\LICENSE.txt" -Force
    # Ship a snapshot; existing OBS plugin configuration is never overwritten.
    $presets = Join-Path $PSScriptRoot 'dist\TinyPTZ-v0.3.4\data\presets.ptz'
    if (Test-Path -LiteralPath $presets) { Copy-Item -LiteralPath $presets -Destination "$release\data\obs-plugins\tiny-ptz\presets.ptz" -Force }
    else { 'TINY_PTZ_PRESETS 2' | Set-Content -LiteralPath "$release\data\obs-plugins\tiny-ptz\presets.ptz" -Encoding utf8 }
    Compress-Archive -LiteralPath src,obs,tests,CMakeLists.txt,build-obs.ps1,docs/OBS-PLUGIN.md,docs/SETTINGS.md -DestinationPath "$release\TinyPTZ-plugin-source.zip" -Force
    Compress-Archive -Path "$release\*" -DestinationPath dist/TinyPTZ-OBS-v0.4.0-Windows-x64.zip -Force
    Write-Host "OBS plugin package: $release"
}
