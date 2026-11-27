[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$GameRoot,
    [ValidateRange(5,120)][int]$TimeoutSeconds = 20
)

$ErrorActionPreference = 'Stop'
$GameRoot = [IO.Path]::GetFullPath($GameRoot)
$client = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\client\powershell\Query-DevBridge.ps1'))
if (-not (Test-Path -LiteralPath $client -PathType Leaf)) { throw "Client not found: $client" }
$powerShell = (Get-Command pwsh -ErrorAction Stop).Source

function Invoke-Bridge(
    [string]$Command,
    [System.Collections.IDictionary]$Payload = @{},
    [string]$Name,
    [double[]]$Start,
    [double[]]$Direction,
    [int]$Flags = 0
) {
    $psi = [Diagnostics.ProcessStartInfo]::new($powerShell)
    $psi.UseShellExecute = $false
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    if ($null -ne $Start -or $null -ne $Direction) {
        if ($Start.Count -ne 3 -or $Direction.Count -ne 3) { throw 'Raycast test vectors must contain three numbers.' }
        $startLiteral = '@(' + (($Start | ForEach-Object { ([double]$_).ToString([Globalization.CultureInfo]::InvariantCulture) }) -join ',') + ')'
        $directionLiteral = '@(' + (($Direction | ForEach-Object { ([double]$_).ToString([Globalization.CultureInfo]::InvariantCulture) }) -join ',') + ')'
        $query = "& $(ConvertTo-PsLiteral $client) -GameRoot $(ConvertTo-PsLiteral $GameRoot) -Command $(ConvertTo-PsLiteral $Command) -Start $startLiteral -Direction $directionLiteral -Flags $Flags -TimeoutSeconds $TimeoutSeconds"
        [void]$psi.ArgumentList.Add('-NoProfile')
        [void]$psi.ArgumentList.Add('-Command')
        [void]$psi.ArgumentList.Add($query)
    } else {
        foreach ($argument in @('-NoProfile', '-File', $client, '-GameRoot', $GameRoot,
            '-Command', $Command, '-TimeoutSeconds', "$TimeoutSeconds")) {
            [void]$psi.ArgumentList.Add($argument)
        }
        if ($Name) { [void]$psi.ArgumentList.Add('-Name'); [void]$psi.ArgumentList.Add($Name) }
        if ($Payload.Count -gt 0) {
            [void]$psi.ArgumentList.Add('-PayloadJson')
            [void]$psi.ArgumentList.Add((ConvertTo-Json -InputObject $Payload -Compress -Depth 12))
        }
    }
    $process = [Diagnostics.Process]::new()
    $process.StartInfo = $psi
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

function ConvertTo-PsLiteral([string]$Value) {
    return "'" + $Value.Replace("'", "''") + "'"
}

$ping = Invoke-Bridge 'ping'
Assert-True ($ping.plugin_version -eq '0.2.0' -and $ping.world_ready -eq $true) 'The v0.2.0 plugin or loaded world was not detected.'
'PASS: ping and loaded world'

$vob = Invoke-Bridge -Command 'vob' -Name 'PC_HERO'
Assert-True ($vob.found -eq $true -and $vob.name -eq 'PC_HERO') 'The legacy vob command did not find PC_HERO.'
'PASS: v0.1.1 vob command'

$player = Invoke-Bridge 'player_state'
$position = [double[]]@($player.position)
Assert-True ($position.Count -eq 3) 'player_state did not return a 3D position.'
$ray = Invoke-Bridge -Command 'raycast' -Start $position -Direction @(0, -1000, 0) -Flags 1058
Assert-True ($ray.hit -eq $true) 'The legacy raycast did not hit the floor below the player.'
'PASS: v0.1.1 raycast command'

$offsetPosition = [double[]]::new(3)
$offsetPosition[0] = [double]$position[0] + 10.0
$offsetPosition[1] = [double]$position[1]
$offsetPosition[2] = [double]$position[2]
$batch = Invoke-Bridge 'raycast_batch' ([ordered]@{
    default_flags = 1058
    probes = @(
        [ordered]@{ start = $position; direction = @(0, -1000, 0) },
        [ordered]@{ start = $offsetPosition; direction = @(0, -1000, 0) }
    )
})
Assert-True ($batch.results.Count -eq 2 -and $batch.results[0].hit -eq $true) 'The legacy raycast_batch response was incomplete.'
'PASS: v0.1.1 raycast_batch command'

$grid = Invoke-Bridge 'surface_grid' ([ordered]@{
    center = $position
    radius_x = 10
    radius_z = 10
    spacing = 10
    down_range = 2000
    include_bbox = $false
})
Assert-True ($grid.results.Count -eq 9) 'The legacy surface_grid did not return all nine expected samples.'
'PASS: v0.1.1 surface_grid command'

$camera = Invoke-Bridge 'camera_get'
Assert-True ($camera.position -and $camera.world_matrix) 'The legacy camera_get response omitted camera transforms.'
'PASS: v0.1.1 camera_get command'

$restore = Invoke-Bridge 'camera_restore'
Assert-True ($restore.ok -eq $true) 'The legacy safe camera_restore request failed.'
'PASS: v0.1.1 camera_restore command'

'Completed 7 backward-compatibility checks.'
