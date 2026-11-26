[CmdletBinding()]
param(
    [string]$ClientScript = (Join-Path $PSScriptRoot '..\client\powershell\Query-DevBridge.ps1')
)

$ErrorActionPreference = 'Stop'
$ClientScript = [IO.Path]::GetFullPath($ClientScript)
if (-not (Test-Path -LiteralPath $ClientScript -PathType Leaf)) {
    throw "Client script not found: $ClientScript"
}
$powerShell = (Get-Command pwsh -ErrorAction Stop).Source
$testRoot = Join-Path ([IO.Path]::GetTempPath()) ("ZenGinDevBridge-lock-test-" + [Guid]::NewGuid().ToString('N'))
$exchangePath = Join-Path $testRoot 'exchange'
[void][IO.Directory]::CreateDirectory($exchangePath)

function Start-Client([string]$ExchangeDirectory, [int]$TimeoutSeconds) {
    $start = [Diagnostics.ProcessStartInfo]::new($powerShell)
    $start.UseShellExecute = $false
    $start.RedirectStandardOutput = $true
    $start.RedirectStandardError = $true
    foreach ($argument in @('-NoProfile', '-File', $ClientScript, '-ExchangeDirectory', $ExchangeDirectory, '-Command', 'ping', '-TimeoutSeconds', "$TimeoutSeconds")) {
        [void]$start.ArgumentList.Add($argument)
    }
    $process = [Diagnostics.Process]::new()
    $process.StartInfo = $start
    [void]$process.Start()
    $process | Add-Member -NotePropertyName StdoutTask -NotePropertyValue $process.StandardOutput.ReadToEndAsync()
    $process | Add-Member -NotePropertyName StderrTask -NotePropertyValue $process.StandardError.ReadToEndAsync()
    return $process
}

try {
    $lockPath = Join-Path $exchangePath 'client.lock'
    $requestPath = Join-Path $exchangePath 'request.json'
    $responsePath = Join-Path $exchangePath 'response.json'

    $owner = Start-Client $exchangePath 30
    $lockDeadline = [DateTime]::UtcNow.AddSeconds(10)
    while (-not (Test-Path -LiteralPath $lockPath) -and [DateTime]::UtcNow -lt $lockDeadline) {
        if ($owner.HasExited) { throw "First client exited before acquiring the lock: $($owner.StderrTask.Result)" }
        Start-Sleep -Milliseconds 25
    }
    if (-not (Test-Path -LiteralPath $lockPath)) { throw 'First client did not create client.lock.' }

    $ownerId = $null
    $readLockDeadline = [DateTime]::UtcNow.AddSeconds(10)
    while ([DateTime]::UtcNow -lt $readLockDeadline) {
        try {
            $ownerId = [IO.File]::ReadAllText($lockPath, [Text.Encoding]::ASCII)
            if ($ownerId) { break }
        } catch [IO.IOException] {
        } catch [UnauthorizedAccessException] {
        }
        Start-Sleep -Milliseconds 25
    }
    if (-not $ownerId) { throw 'First client did not finish writing its lock owner ID.' }
    $requestDeadline = [DateTime]::UtcNow.AddSeconds(10)
    while (-not (Test-Path -LiteralPath $requestPath) -and [DateTime]::UtcNow -lt $requestDeadline) {
        if ($owner.HasExited) { throw "First client exited before publishing its request: $($owner.StderrTask.Result)" }
        Start-Sleep -Milliseconds 25
    }
    if (-not (Test-Path -LiteralPath $requestPath)) { throw 'First client did not publish request.json.' }
    $ownerRequest = Get-Content -LiteralPath $requestPath -Raw -Encoding UTF8 | ConvertFrom-Json
    if ($ownerRequest.id -ne $ownerId) { throw 'client.lock does not identify the first client request.' }

    $competitor = Start-Client $exchangePath 5
    if (-not $competitor.WaitForExit(10000)) {
        $competitor.Kill($true)
        throw 'Second client did not fail promptly while the first client held the lock.'
    }
    $competitorStdout = $competitor.StdoutTask.Result
    $competitorStderr = $competitor.StderrTask.Result
    if ($competitor.ExitCode -eq 0) {
        throw 'Second client unexpectedly completed while the first client held the lock.'
    }
    if (-not (Test-Path -LiteralPath $lockPath)) {
        throw 'Second client removed client.lock even though it did not acquire the lock.'
    }
    if ([IO.File]::ReadAllText($lockPath, [Text.Encoding]::ASCII) -ne $ownerId) {
        throw 'Second client changed the lock owner ID.'
    }
    $currentRequest = Get-Content -LiteralPath $requestPath -Raw -Encoding UTF8 | ConvertFrom-Json
    if ($currentRequest.id -ne $ownerId) { throw 'Second client replaced the first client request.' }

    $response = @{
        id = $ownerId; command = 'ping'; ok = $true
        protocol_version = 1; plugin_version = 'mock'
        plugin = 'ZenGinDevBridge'; world_ready = $false
    }
    [IO.File]::WriteAllText($responsePath, ($response | ConvertTo-Json -Compress), [Text.UTF8Encoding]::new($false))
    if (-not $owner.WaitForExit(10000)) {
        $owner.Kill($true)
        throw 'First client did not accept its matching response.'
    }
    if ($owner.ExitCode -ne 0) { throw "First client failed: $($owner.StderrTask.Result)" }
    if (Test-Path -LiteralPath $lockPath) { throw 'Lock owner did not clean up its own lock file.' }

    'PASS: a competing client cannot remove or replace the active client lock/request'
    'PASS: the lock owner completes normally and removes its lock'
} finally {
    foreach ($process in @($owner, $competitor)) {
        if ($null -ne $process) {
            if (-not $process.HasExited) { $process.Kill($true) }
            $process.Dispose()
        }
    }
    $resolvedRoot = [IO.Path]::GetFullPath($testRoot)
    $tempRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath())
    if ($resolvedRoot.StartsWith($tempRoot, [StringComparison]::OrdinalIgnoreCase) -and
        (Split-Path -Parent $resolvedRoot) -eq $tempRoot.TrimEnd([IO.Path]::DirectorySeparatorChar)) {
        Remove-Item -LiteralPath $resolvedRoot -Recurse -Force
    } else {
        throw 'Refusing to remove a test directory outside the system temporary directory.'
    }
}
