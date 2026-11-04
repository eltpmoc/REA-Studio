# SPDX-License-Identifier: GPL-3.0-or-later
[CmdletBinding()]
param(
    [string]$Build = "build-windows",
    [Parameter(Mandatory=$true)][string]$QtRoot,
    [Parameter(Mandatory=$true)][string]$QtSource,
    [string]$Output = "dist"
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if (-not $IsWindows -or -not [Environment]::Is64BitProcess) { throw 'Use PowerShell 7 on real Windows x64.' }
$repo = Split-Path $PSScriptRoot -Parent
$buildDir = (Resolve-Path $Build).Path
$qt = (Resolve-Path $QtRoot).Path
$qtArchive = (Resolve-Path $QtSource).Path
$metadata = Get-Content (Join-Path $buildDir 'REA_BUILD_METADATA.txt') -Raw
if ($metadata -notmatch 'Qt=6\.8\.3' -or $metadata -notmatch 'PointerBits=64' -or $metadata -notmatch 'Compiler=MSVC') {
    throw 'The alpha Windows package requires MSVC x64 and pinned Qt 6.8.3.'
}
function Assert-Native([string]$Step) { if ($LASTEXITCODE -ne 0) { throw "$Step failed: exit $LASTEXITCODE" } }
ctest --test-dir $buildDir -C Release --output-on-failure --output-junit windows-release-tests.xml
Assert-Native 'CTest Windows Release'
python (Join-Path $PSScriptRoot 'release.py') source --output $Output
Assert-Native 'Source/license audit'
$outDir = (Resolve-Path $Output).Path
$base = 'REA-Studio-v0.3.0-alpha-Windows-x64'
$stagingParent = Join-Path $buildDir ('package-' + [Guid]::NewGuid().ToString('N'))
$stage = Join-Path $stagingParent $base
cmake --install $buildDir --config Release --prefix $stage
Assert-Native 'Install Windows'
$bin = Join-Path $stage 'bin'
$exe = Join-Path $bin 'rea-studio.exe'
# Restrict deployment to QtBase; do not redistribute developer CRT DLLs.
& (Join-Path $qt 'bin/windeployqt.exe') --release --no-plugins --no-translations --no-compiler-runtime --no-opengl-sw --no-system-d3d-compiler --dir $bin $exe
Assert-Native 'windeployqt'
$platforms = Join-Path $bin 'platforms'
New-Item -ItemType Directory -Force $platforms | Out-Null
foreach ($plugin in @('qwindows.dll', 'qoffscreen.dll')) {
    Copy-Item (Join-Path $qt "plugins/platforms/$plugin") (Join-Path $platforms $plugin)
}
@'
[Paths]
Prefix=.
Plugins=.
Libraries=.
'@ | Set-Content (Join-Path $bin 'qt.conf') -Encoding ascii
foreach ($dll in @('Qt6Core.dll', 'Qt6Gui.dll', 'Qt6Widgets.dll', 'platforms/qwindows.dll', 'platforms/qoffscreen.dll')) {
    if (-not (Test-Path (Join-Path $bin $dll))) { throw "Required deployment DLL missing: $dll" }
}
$qtDlls = @(Get-ChildItem $bin -Filter 'Qt6*.dll' -Recurse)
foreach ($dll in $qtDlls) {
    if ($dll.Name -notin @('Qt6Core.dll', 'Qt6Gui.dll', 'Qt6Widgets.dll')) { throw "Unreviewed Qt module: $($dll.Name)" }
}
if (Get-ChildItem $bin -Recurse -File | Where-Object { $_.Name -match '^(vcruntime|msvcp|concrt).*\.dll$' }) {
    throw 'Use the official Visual C++ Redistributable prerequisite; do not bundle developer CRT DLLs.'
}
$shared = Join-Path $stage 'share/rea-studio'
$sources = Join-Path $shared 'sources'
New-Item -ItemType Directory -Force $sources | Out-Null
Copy-Item (Join-Path $outDir 'REA-Studio-v0.3.0-alpha-Source.tar.gz') $sources
Copy-Item $qtArchive $sources
python (Join-Path $PSScriptRoot 'release.py') qt-notices --qt-source $qtArchive --output (Join-Path $shared 'licenses/Qt')
Assert-Native 'Qt SHA-256 and notices'
Copy-Item (Join-Path $repo 'packaging/RELEASE-NOTES.md') (Join-Path $shared 'RELEASE-NOTES.md')
# MSVC dumpbin provides an inspectable direct-DLL dependency report.
$dumpbin = Get-Command dumpbin.exe -ErrorAction Stop
& $dumpbin.Source /DEPENDENTS $exe | Set-Content (Join-Path $buildDir 'windows-dependencies.txt')
Assert-Native 'dumpbin'
Get-ChildItem $bin -Recurse -File | ForEach-Object {
    '{0}  {1}' -f (Get-FileHash $_.FullName -Algorithm SHA256).Hash.ToLower(), $_.FullName.Substring($stage.Length + 1)
} | Set-Content (Join-Path $shared 'DEPLOYMENT-SHA256SUMS.txt') -Encoding utf8
$oldPath = $env:PATH
$oldPlatform = $env:QT_QPA_PLATFORM
$oldPluginPath = $env:QT_PLUGIN_PATH
$oldPlatformPath = $env:QT_QPA_PLATFORM_PLUGIN_PATH
try {
    $env:PATH = "$bin;$env:SystemRoot\System32;$env:SystemRoot"
    $env:QT_PLUGIN_PATH = $null
    $env:QT_QPA_PLATFORM_PLUGIN_PATH = $null
    foreach ($backend in @('offscreen', 'windows')) {
        $env:QT_QPA_PLATFORM = $backend
        & $exe --smoke (Join-Path $buildDir 'smoke.elf')
        Assert-Native "Packaged smoke ($backend, no Qt development PATH)"
    }
} finally {
    $env:PATH = $oldPath
    $env:QT_QPA_PLATFORM = $oldPlatform
    $env:QT_PLUGIN_PATH = $oldPluginPath
    $env:QT_QPA_PLATFORM_PLUGIN_PATH = $oldPlatformPath
}
$zip = Join-Path $outDir "$base.zip"
if (Test-Path $zip) { throw "Refusing to replace existing archive: $zip" }
Compress-Archive -Path $stage -DestinationPath $zip -CompressionLevel Optimal
Copy-Item (Join-Path $repo 'packaging/RELEASE-NOTES.md') (Join-Path $outDir 'RELEASE-NOTES.md')
python (Join-Path $PSScriptRoot 'release.py') checksums --output $outDir
Assert-Native 'SHA256SUMS'
Write-Host "PASS Windows deployment/package: $zip"
