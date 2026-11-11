$ErrorActionPreference='Stop'
$vc='C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231'
$kit='C:\Program Files (x86)\Windows Kits\10'
$env:INCLUDE="$vc\include;$kit\Include\10.0.26100.0\ucrt;$kit\Include\10.0.26100.0\shared;$kit\Include\10.0.26100.0\um;$kit\Include\10.0.26100.0\winrt"
$env:LIB="$vc\lib\x64;$kit\Lib\10.0.26100.0\ucrt\x64;$kit\Lib\10.0.26100.0\um\x64"
New-Item -ItemType Directory -Force build | Out-Null
& "$vc\bin\Hostx64\x64\cl.exe" /nologo /EHsc /std:c++17 /MT tools/probe.cpp /Fobuild/probe.obj /Febuild/probe.exe ole32.lib oleaut32.lib strmiids.lib
if($LASTEXITCODE -ne 0){throw 'Compile failed'}
& .\build\probe.exe
