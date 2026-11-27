[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$GameRoot,
    [ValidateRange(5,120)][int]$TimeoutSeconds = 20,
    [ValidateRange(100,30000)][int]$MoveDurationMs = 800
)

$ErrorActionPreference = 'Stop'
$GameRoot = [IO.Path]::GetFullPath($GameRoot)
$client = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\client\powershell\Query-DevBridge.ps1'))
$configPath = Join-Path $GameRoot 'ZenGinDevBridge.config.json'
if (-not (Test-Path -LiteralPath $client -PathType Leaf)) { throw "Client not found: $client" }
if (-not (Test-Path -LiteralPath $configPath -PathType Leaf)) { throw "Local configuration not found: $configPath" }

$config = Get-Content -LiteralPath $configPath -Raw -Encoding UTF8 | ConvertFrom-Json
if ($config.allow_player_control -ne $true) {
    throw 'This test requires allow_player_control:true in the isolated testbed configuration.'
}

$powerShell = (Get-Command pwsh -ErrorAction Stop).Source

function Invoke-Bridge(
    [string]$Command,
    [System.Collections.IDictionary]$Payload = @{}
) {
    $start = [Diagnostics.ProcessStartInfo]::new($powerShell)
    $start.UseShellExecute = $false
    $start.RedirectStandardOutput = $true
    $start.RedirectStandardError = $true
    foreach ($argument in @('-NoProfile', '-File', $client, '-GameRoot', $GameRoot,
        '-Command', $Command, '-TimeoutSeconds', "$TimeoutSeconds")) {
        [void]$start.ArgumentList.Add($argument)
    }
    if ($Payload.Count -gt 0) {
        $payloadJson = ConvertTo-Json -InputObject $Payload -Compress -Depth 8
        [void]$start.ArgumentList.Add('-PayloadJson')
        [void]$start.ArgumentList.Add($payloadJson)
    }
    $process = [Diagnostics.Process]::new()
    $process.StartInfo = $start
    [void]$process.Start()
    $stdout = $process.StandardOutput.ReadToEndAsync()
    $stderr = $process.StandardError.ReadToEndAsync()
    if (-not $process.WaitForExit(($TimeoutSeconds + 10) * 1000)) {
        $process.Kill($true)
        throw "$Command client subprocess exceeded its safety timeout."
    }
    try { $response = $stdout.Result | ConvertFrom-Json } catch {
        throw "$Command returned no valid JSON. $($stderr.Result) $($stdout.Result)"
    }
    if ($process.ExitCode -ne 0 -or -not $response.ok) {
        throw "$Command failed: $($response.error_code) $($response.error) $($stderr.Result)"
    }
    return $response
}

function Assert-True([bool]$Value, [string]$Message) {
    if (-not $Value) { throw $Message }
}

$ping = Invoke-Bridge 'ping'
Assert-True ($ping.plugin_version -eq '0.2.0') "Expected plugin 0.2.0; got $($ping.plugin_version)."
Assert-True ($ping.capabilities.mutating.player_control -eq $true) 'The running plugin does not report player control as enabled.'

$before = Invoke-Bridge 'player_state'
Assert-True ($before.dialogue.active -eq $false) 'Finish the current dialogue before running movement checks.'
Assert-True ($before.collision_support.supported -eq $true) 'The player must stand on supported ground before running movement checks.'

$collision = Invoke-Bridge 'player_collision'
$clearProbe = @($collision.obstacle_probes | Where-Object { $_.ray.ok -eq $true -and $_.ray.hit -eq $false } | Select-Object -First 1)
if ($clearProbe.Count -eq 0) { throw 'No unobstructed direction was found near the player.' }

$direction = [string]$clearProbe[0].direction
$moveDirection = switch ($direction) {
    'forward' { 'forward' }
    'backward' { 'backward' }
    'right' { 'strafe_right' }
    'left' { 'strafe_left' }
    default { throw "Unsupported collision-probe direction: $direction" }
}

$basis = if ($direction -in @('forward','backward')) {
    @($before.orientation.at)
} else {
    @($before.orientation.right)
}
$sign = if ($direction -in @('backward','left')) { -1.0 } else { 1.0 }
$invariant = [Globalization.CultureInfo]::InvariantCulture
$position = @($before.position)
$unitX = [double]::Parse([string]$basis[0], $invariant) * $sign
$unitZ = [double]::Parse([string]$basis[2], $invariant) * $sign
$startX = [double]::Parse([string]$position[0], $invariant)
$startY = [double]::Parse([string]$position[1], $invariant)
$startZ = [double]::Parse([string]$position[2], $invariant)
$targetDistance = 200.0
$target = @(
    ($startX + ($unitX * $targetDistance)),
    $startY,
    ($startZ + ($unitZ * $targetDistance))
)

$move = Invoke-Bridge 'player_move' ([ordered]@{
    direction = $moveDirection
    duration_ms = $MoveDurationMs
})
Assert-True ($move.control_ai_frames -gt 0) 'The player AI hook did not observe any movement frames.'
Assert-True ($move.distance_moved -gt 1.0) "The player did not move in a clear direction ($direction)."
Assert-True ($move.input_released -eq $true) 'player_move did not restore the previous input state.'
Assert-True ($move.world_matched -eq $true) 'The active world changed during player_move.'

$walk = Invoke-Bridge 'player_walk_to' ([ordered]@{
    target = $target
    duration_ms = 5000
    tolerance = 45
    stagnation_ms = 1500
})
Assert-True ($walk.reason -eq 'target_reached' -and $walk.target_reached -eq $true) `
    "player_walk_to did not reach the target tolerance: $($walk.reason)."
Assert-True ($walk.control_ai_frames -gt 0) 'player_walk_to did not run through player AI frames.'
Assert-True ($walk.distance_moved -gt 1.0) 'player_walk_to reported success without measurable movement.'
Assert-True ($walk.input_released -eq $true) 'player_walk_to did not restore the previous input state.'

$after = Invoke-Bridge 'player_state'
Assert-True ($after.collision_support.supported -eq $true) 'The player is not supported by the world after movement.'

[pscustomobject]@{
    plugin_version = $ping.plugin_version
    direction = $direction
    move_distance = $move.distance_moved
    move_ai_frames = $move.control_ai_frames
    walk_distance = $walk.distance_moved
    walk_ai_frames = $walk.control_ai_frames
    target_distance = $walk.target_distance
    target_reached = $walk.target_reached
    input_released = ($move.input_released -and $walk.input_released)
} | ConvertTo-Json -Depth 4
