[CmdletBinding()]
param(
    [string]$ToolchainRoot = $env:DEVBRIDGE_TOOLCHAIN_ROOT,
    [string]$CMakePath = $env:DEVBRIDGE_CMAKE,
    [string]$NinjaPath = $env:DEVBRIDGE_NINJA,
    [string]$WindowsSdkRoot = $env:DEVBRIDGE_WINDOWS_SDK_ROOT,
    [string]$WindowsSdkVersion
)

$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))

if (-not $CMakePath) {
    $cmakeCommand = Get-Command cmake -ErrorAction SilentlyContinue
    if ($cmakeCommand) { $CMakePath = $cmakeCommand.Source }
}
if (-not $NinjaPath) {
    $ninjaCommand = Get-Command ninja -ErrorAction SilentlyContinue
    if ($ninjaCommand) { $NinjaPath = $ninjaCommand.Source }
}
if (-not $CMakePath -or -not (Test-Path -LiteralPath $CMakePath -PathType Leaf)) {
    throw 'Provide -CMakePath or set DEVBRIDGE_CMAKE to a CMake executable.'
}
if (-not $NinjaPath -or -not (Test-Path -LiteralPath $NinjaPath -PathType Leaf)) {
    throw 'Provide -NinjaPath or set DEVBRIDGE_NINJA to a Ninja executable.'
}
if (-not $ToolchainRoot -or -not (Test-Path -LiteralPath $ToolchainRoot -PathType Container)) {
    throw 'Provide -ToolchainRoot or set DEVBRIDGE_TOOLCHAIN_ROOT to an x86 MSVC and Windows SDK toolchain root.'
}

$vcBase = Join-Path $ToolchainRoot 'VC\Tools\MSVC'
$vcDirectory = Get-ChildItem -LiteralPath $vcBase -Directory |
    Sort-Object { [version]$_.Name } -Descending |
    Select-Object -First 1
if (-not $vcDirectory) { throw "MSVC toolset not found under $vcBase" }
$vcRoot = $vcDirectory.FullName

$sdkRoot = $WindowsSdkRoot
if (-not $sdkRoot) { $sdkRoot = Join-Path $ToolchainRoot 'Windows Kits\10' }
if (-not (Test-Path -LiteralPath $sdkRoot -PathType Container)) {
    throw 'Provide -WindowsSdkRoot or set DEVBRIDGE_WINDOWS_SDK_ROOT to the Windows Kits\10 directory.'
}
$sdkInclude = Join-Path $sdkRoot 'Include'
if (-not $WindowsSdkVersion) {
    $sdkVersionDirectory = Get-ChildItem -LiteralPath $sdkInclude -Directory |
        Sort-Object { [version]$_.Name } -Descending |
        Select-Object -First 1
    if (-not $sdkVersionDirectory) { throw "Windows SDK headers not found under $sdkInclude" }
    $WindowsSdkVersion = $sdkVersionDirectory.Name
}

$sdkVersionRoot = Join-Path $sdkInclude $WindowsSdkVersion
$sdkLibraryRoot = Join-Path (Join-Path $sdkRoot 'Lib') $WindowsSdkVersion
$compilerDirectory = Join-Path $vcRoot 'bin\Hostx64\x86'
$sdkBin = Join-Path $sdkRoot "bin\$WindowsSdkVersion\x86"
$cl = Join-Path $compilerDirectory 'cl.exe'
$includePaths = @(
    (Join-Path $vcRoot 'include'),
    (Join-Path $sdkVersionRoot 'ucrt'),
    (Join-Path $sdkVersionRoot 'shared'),
    (Join-Path $sdkVersionRoot 'um')
)
$libraryPaths = @(
    (Join-Path $vcRoot 'lib\x86'),
    (Join-Path $sdkLibraryRoot 'ucrt\x86'),
    (Join-Path $sdkLibraryRoot 'um\x86')
)
foreach ($path in @($cl, (Join-Path $sdkBin 'rc.exe')) + $includePaths + $libraryPaths) {
    if (-not (Test-Path -LiteralPath $path)) { throw "Required x86 toolchain path is missing: $path" }
}

$env:Path = "$compilerDirectory;$sdkBin;$(Split-Path -Parent $NinjaPath);$(Split-Path -Parent $CMakePath);$env:Path"
$env:INCLUDE = $includePaths -join ';'
$env:LIB = $libraryPaths -join ';'
$env:VCToolsInstallDir = "$vcRoot\"
$env:WindowsSdkDir = "$sdkRoot\"
$env:WindowsSDKVersion = "$WindowsSdkVersion\"

$buildDirectory = Join-Path $projectRoot 'build\g2a-release'
$configureArguments = @(
    '-S', $projectRoot,
    '-B', $buildDirectory,
    '-G', 'Ninja',
    "-DCMAKE_MAKE_PROGRAM=$NinjaPath",
    "-DCMAKE_CXX_COMPILER=$cl",
    '-DCMAKE_BUILD_TYPE=Release'
)
& $CMakePath @configureArguments
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed with exit code $LASTEXITCODE" }

& $CMakePath --build $buildDirectory --target ZenGinDevBridge --parallel 2
if ($LASTEXITCODE -ne 0) { throw "Build failed with exit code $LASTEXITCODE" }

$dll = Join-Path $buildDirectory 'ZenGinDevBridge.dll'
if (-not (Test-Path -LiteralPath $dll -PathType Leaf)) { throw "Build completed without producing $dll" }
Get-FileHash -LiteralPath $dll -Algorithm SHA256
Write-Output "Build output: $dll"
