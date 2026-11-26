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
$testRoot = Join-Path ([IO.Path]::GetTempPath()) ("ZenGinDevBridge-test-" + [Guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($testRoot)

function Start-MockBridge([string]$ExchangePath, [string]$Mode) {
    Start-Job -ArgumentList $ExchangePath, $Mode -ScriptBlock {
        param($dir, $mode)
        $requestPath = Join-Path $dir 'request.json'
        $responsePath = Join-Path $dir 'response.json'
        $capturePath = Join-Path $dir 'captured-request.json'
        $deadline = [DateTime]::UtcNow.AddSeconds(15)
        while ([DateTime]::UtcNow -lt $deadline) {
            if (Test-Path -LiteralPath $requestPath) {
                try {
                    $request = Get-Content -LiteralPath $requestPath -Raw -Encoding UTF8 | ConvertFrom-Json
                    [IO.File]::WriteAllText($capturePath, ($request | ConvertTo-Json -Compress -Depth 12), [Text.UTF8Encoding]::new($false))
                    if ($mode -eq 'ping') {
                        if ($request.protocol_version -ne 1 -or $request.expires_at_unix_ms -le [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()) {
                            throw 'Client omitted protocol version or a future expiry.'
                        }
                        $response = @{
                            id = $request.id; command = $request.command; ok = $true
                            protocol_version = 1; plugin_version = 'mock'
                            plugin = 'ZenGinDevBridge'; world_ready = $false
                        }
                        [IO.File]::WriteAllText($responsePath, ($response | ConvertTo-Json -Compress), [Text.UTF8Encoding]::new($false))
                        Remove-Item -LiteralPath $requestPath -Force
                        return
                    }
                    $cancelPath = Join-Path $dir ("cancel." + $request.id)
                    if (Test-Path -LiteralPath $cancelPath) {
                        $response = @{
                            id = $request.id; command = $request.command; ok = $false
                            protocol_version = 1; plugin_version = 'mock'
                            error_code = 'cancelled'; error = 'request cancelled by client'
                        }
                        [IO.File]::WriteAllText($responsePath, ($response | ConvertTo-Json -Compress), [Text.UTF8Encoding]::new($false))
                        Remove-Item -LiteralPath $requestPath -Force
                        return
                    }
                } catch {
                    throw "Mock bridge failed: $($_.Exception.Message)"
                }
            }
            Start-Sleep -Milliseconds 50
        }
        throw 'Mock bridge timed out waiting for a request or cancellation.'
    }
}

function Invoke-Client([string]$ExchangePath, [int]$TimeoutSeconds) {
    $start = [Diagnostics.ProcessStartInfo]::new($powerShell)
    $start.UseShellExecute = $false
    $start.RedirectStandardOutput = $true
    $start.RedirectStandardError = $true
    foreach ($argument in @('-NoProfile', '-File', $ClientScript, '-ExchangeDirectory', $ExchangePath, '-Command', 'ping', '-TimeoutSeconds', "$TimeoutSeconds")) {
        [void]$start.ArgumentList.Add($argument)
    }
    $process = [Diagnostics.Process]::new()
    $process.StartInfo = $start
    [void]$process.Start()
    $stdout = $process.StandardOutput.ReadToEndAsync()
    $stderr = $process.StandardError.ReadToEndAsync()
    if (-not $process.WaitForExit(20000)) {
        $process.Kill($true)
        throw 'Client subprocess exceeded 20 seconds.'
    }
    [pscustomobject]@{ ExitCode = $process.ExitCode; Stdout = $stdout.Result; Stderr = $stderr.Result }
}

try {
    $pingPath = Join-Path $testRoot 'ping'
    [void][IO.Directory]::CreateDirectory($pingPath)
    $pingJob = Start-MockBridge $pingPath 'ping'
    $ping = Invoke-Client $pingPath 5
    Wait-Job $pingJob -Timeout 5 | Out-Null
    if ($pingJob.State -ne 'Completed') { throw 'Mock ping responder did not complete.' }
    $pingResponse = $ping.Stdout | ConvertFrom-Json
    if ($ping.ExitCode -ne 0 -or -not $pingResponse.ok -or $pingResponse.plugin -ne 'ZenGinDevBridge') {
        throw "Client ping roundtrip failed: $($ping.Stderr)"
    }
    $capturedPing = Get-Content -LiteralPath (Join-Path $pingPath 'captured-request.json') -Raw | ConvertFrom-Json
    if ($capturedPing.protocol_version -ne 1 -or $capturedPing.expires_at_unix_ms -le [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()) {
        throw 'Client ping request did not contain a valid protocol version and expiry.'
    }
    Remove-Job $pingJob -Force

    $cancelPath = Join-Path $testRoot 'cancel'
    [void][IO.Directory]::CreateDirectory($cancelPath)
    $cancelJob = Start-MockBridge $cancelPath 'cancel'
    $cancel = Invoke-Client $cancelPath 1
    Wait-Job $cancelJob -Timeout 5 | Out-Null
    if ($cancelJob.State -ne 'Completed') { throw 'Mock cancellation responder did not complete.' }
    $cancelResponse = $cancel.Stdout | ConvertFrom-Json
    if ($cancel.ExitCode -ne 2 -or $cancelResponse.error_code -ne 'cancelled') {
        throw "Client cancellation roundtrip failed: $($cancel.Stderr)"
    }
    Remove-Job $cancelJob -Force

    'PASS: protocol version and expiry roundtrip'
    'PASS: client timeout sends cancellation and accepts the cancellation response'
} finally {
    Get-Job | Where-Object { $_.State -in @('Running','NotStarted') } | Stop-Job -ErrorAction SilentlyContinue
    Get-Job | Where-Object { $_.Name -like 'Job*' } | Remove-Job -Force -ErrorAction SilentlyContinue
    $resolvedRoot = [IO.Path]::GetFullPath($testRoot)
    $tempRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath())
    if ($resolvedRoot.StartsWith($tempRoot, [StringComparison]::OrdinalIgnoreCase) -and
        (Split-Path -Parent $resolvedRoot) -eq $tempRoot.TrimEnd([IO.Path]::DirectorySeparatorChar)) {
        Remove-Item -LiteralPath $resolvedRoot -Recurse -Force
    } else {
        throw 'Refusing to remove a test directory outside the system temporary directory.'
    }
}

