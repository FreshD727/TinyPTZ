param(
    [string]$ObsRoot = 'C:\Program Files\obs-studio',
    [switch]$Portable
)
$ErrorActionPreference = 'Stop'
if (Get-Process obs64 -ErrorAction SilentlyContinue) { throw 'Close OBS Studio before installing the plugin, then run this script again.' }
$runtime = Join-Path $ObsRoot 'bin\64bit\Qt6Core.dll'
if (-not (Test-Path -LiteralPath $runtime)) { throw 'OBS not found. Pass -ObsRoot with the correct OBS installation folder.' }
$qt = (Get-Item -LiteralPath $runtime).VersionInfo.ProductVersion
$obsVersion = (Get-Item -LiteralPath (Join-Path $ObsRoot 'bin\64bit\obs64.exe')).VersionInfo.ProductVersion
if ($obsVersion -notlike '31.*') { throw "This build targets OBS 31.x. Found OBS $obsVersion." }
if ($qt -notlike '6.6.3*') { throw "This plugin build requires the OBS 31 Qt 6.6.3 runtime. Found $qt. It must be rebuilt for another Qt version." }
$release = $PSScriptRoot
if ($Portable) {
    $binary = Join-Path $ObsRoot 'obs-plugins\64bit'
    $data = Join-Path $ObsRoot 'data\obs-plugins\tiny-ptz'
} else {
    $plugin = Join-Path $env:ProgramData 'obs-studio\plugins\tiny-ptz'
    $binary = Join-Path $plugin 'bin\64bit'
    $data = Join-Path $plugin 'data'
}
New-Item -ItemType Directory -Force $binary,$data | Out-Null
Copy-Item -LiteralPath (Join-Path $release 'obs-plugins\64bit\tiny-ptz.dll') -Destination $binary -Force
Copy-Item -LiteralPath (Join-Path $release 'data\obs-plugins\tiny-ptz\presets.ptz') -Destination $data -Force
Write-Host 'Tiny PTZ installed. Start OBS and enable Docks > Tiny PTZ.'
Write-Host 'Existing user presets and scene assignments have been preserved.'
