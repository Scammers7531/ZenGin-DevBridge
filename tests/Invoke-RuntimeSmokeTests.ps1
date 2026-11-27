[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$GameRoot,
    [switch]$IncludePlayerControl,
    [ValidateRange(5,120)][int]$TimeoutSeconds = 20
)

$ErrorActionPreference = 'Stop'
$GameRoot = [IO.Path]::GetFullPath($GameRoot)
$client = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\client\powershell\Query-DevBridge.ps1'))
if (-not (Test-Path -LiteralPath $client -PathType Leaf)) { throw "Client not found: $client" }
$powerShell = (Get-Command pwsh -ErrorAction Stop).Source
$exchangeName = 'ZenGinDevBridge'
$configPath = Join-Path $GameRoot 'ZenGinDevBridge.config.json'
if (Test-Path -LiteralPath $configPath) {
    $config = Get-Content -LiteralPath $configPath -Raw -Encoding UTF8 | ConvertFrom-Json
    if ($config.exchange_directory) { $exchangeName = [string]$config.exchange_directory }
}
$exchangeRoot = [IO.Path]::GetFullPath((Join-Path $GameRoot $exchangeName))
$passed = [System.Collections.Generic.List[string]]::new()

function Invoke-Bridge(
    [string]$Command,
    [System.Collections.IDictionary]$Payload = @{},
    [switch]$AllowError,
    [int]$ClientTimeoutSeconds = $TimeoutSeconds
) {
    $start = [Diagnostics.ProcessStartInfo]::new($powerShell)
    $start.UseShellExecute = $false
    $start.RedirectStandardOutput = $true
    $start.RedirectStandardError = $true
    foreach ($argument in @('-NoProfile', '-File', $client, '-GameRoot', $GameRoot,
        '-Command', $Command, '-TimeoutSeconds', "$ClientTimeoutSeconds")) {
        [void]$start.ArgumentList.Add($argument)
    }
    if ($Payload.Count -gt 0) {
        $payloadJson = ConvertTo-Json -InputObject $Payload -Compress -Depth 12
        [void]$start.ArgumentList.Add('-PayloadJson')
        [void]$start.ArgumentList.Add($payloadJson)
    }
    $process = [Diagnostics.Process]::new()
    $process.StartInfo = $start
    [void]$process.Start()
    $stdout = $process.StandardOutput.ReadToEndAsync()
    $stderr = $process.StandardError.ReadToEndAsync()
    if (-not $process.WaitForExit(($ClientTimeoutSeconds + 10) * 1000)) {
        $process.Kill($true)
        throw "$Command client subprocess exceeded its safety timeout."
    }
    $response = $null
    try { $response = $stdout.Result | ConvertFrom-Json } catch {
        throw "$Command returned no valid JSON. $($stderr.Result) $($stdout.Result)"
    }
    if (-not $AllowError -and ($process.ExitCode -ne 0 -or -not $response.ok)) {
        throw "$Command failed: $($response.error_code) $($response.error) $($stderr.Result)"
    }
    return $response
}

function Assert-True([bool]$Value, [string]$Message) {
    if (-not $Value) { throw $Message }
}

function Record-Pass([string]$Name) {
    $passed.Add($Name)
    "PASS: $Name"
}

$ping = Invoke-Bridge 'ping'
Assert-True ($ping.world_ready -eq $true) 'Gothic did not report a loaded world.'
Assert-True ($ping.plugin_version -eq '0.2.0') "Expected plugin 0.2.0; got $($ping.plugin_version)."
foreach ($capability in @('vob_search','npc_inspect','player_collision','waynet_local_graph',
        'daedalus_read','mover_inspect','monitor','screenshot')) {
    Assert-True ($ping.capabilities.read_only.$capability -eq $true) "Missing capability: $capability"
}
Record-Pass 'ping, world identity, v0.2 capabilities'

$vobSearch = Invoke-Bridge 'vob_search' ([ordered]@{ limit = 3; scan_limit = 20000 })
Assert-True ($vobSearch.results.Count -gt 0) 'VOb search returned no rows.'
Assert-True ($null -ne $vobSearch.results[0].visual) 'VOb summary omitted visual metadata.'
$npcSearch = Invoke-Bridge 'npc_search' ([ordered]@{ limit = 3; scan_limit = 20000 })
Assert-True ($npcSearch.results.Count -gt 0) 'NPC search returned no rows.'
Record-Pass 'bounded VOb/NPC search and visual metadata'

$player = Invoke-Bridge 'player_state'
Assert-True ($player.player -eq $true -and $player.name) 'Player state did not identify the active hero.'
$npc = Invoke-Bridge 'npc_inspect' ([ordered]@{ name = $player.name; inventory_limit = 4 })
Assert-True ($npc.life -and $npc.ai -and $npc.combat -and $npc.inventory -and $npc.animation -and $npc.dialogue) `
    'NPC inspection omitted one or more diagnostic groups.'
$collision = Invoke-Bridge 'player_collision'
Assert-True ($null -ne $collision.character_support -and $null -ne $collision.obstacle_probes) `
    'Player collision response omitted support or obstacle probes.'
Record-Pass 'NPC, player movement, collision support and obstacle probes'

$waypoints = Invoke-Bridge 'waypoint_search' ([ordered]@{ limit = 8 })
Assert-True ($waypoints.results.Count -gt 0) 'WayNet waypoint search returned no rows.'
$waypoint = $waypoints.results[0]
$waypointName = [string]$waypoint.PSObject.Properties['name'].Value
Assert-True (-not [string]::IsNullOrWhiteSpace($waypointName)) 'WayNet result omitted the waypoint name.'
$inspectedWaypoint = Invoke-Bridge 'waypoint_inspect' ([ordered]@{ name = $waypointName })
Assert-True ($null -ne $inspectedWaypoint.connections) 'Waypoint inspection omitted its edges.'
$nearest = Invoke-Bridge 'waynet_nearest' ([ordered]@{ position = $player.position })
$localGraph = Invoke-Bridge 'waynet_local_graph' ([ordered]@{ center = $nearest.name; depth = 2; max_nodes = 32 })
$waynet = Invoke-Bridge 'waynet_check' ([ordered]@{ component_scan_limit = 100000 })
Assert-True ($localGraph.node_count -gt 0 -and $null -ne $waynet.weak_component_count) `
    'WayNet local graph or connected-component analysis returned no summary.'
$adjacent = @($inspectedWaypoint.connections | Where-Object { $_.target -and $_.target.name }) | Select-Object -First 1
$routeTarget = if ($null -ne $adjacent) { $adjacent.target.name } else { $waypointName }
$route = Invoke-Bridge 'waynet_route' ([ordered]@{ start = $waypointName; target = $routeTarget; max_nodes = 20000 })
Assert-True ($route.found -eq $true) 'WayNet route could not find a direct adjacent route.'
Record-Pass 'WayNet search, edges, nearest, local graph, islands and route'

$symbols = Invoke-Bridge 'daedalus_search' ([ordered]@{ limit = 4; scan_limit = 50000 })
Assert-True ($symbols.results.Count -gt 0) 'Daedalus symbol metadata search returned no results.'
$symbolRead = Invoke-Bridge 'daedalus_read' ([ordered]@{ symbol = 'TRUE'; count = 1 })
Assert-True ($symbolRead.value_available -eq $true -and $symbolRead.type -eq 'int' -and $symbolRead.values.Count -eq 1) 'The built-in scalar Daedalus int read failed.'
$typeRead = Invoke-Bridge 'daedalus_read' ([ordered]@{ symbol = 'NPC_ISDEAD' })
Assert-True ($typeRead.value_available -eq $false -and $typeRead.value_reason) 'Metadata-only symbol was not safely declined.'
Record-Pass 'Daedalus metadata-only type guard'
Record-Pass 'Daedalus metadata search and typed scalar read'
$missingSymbol = Invoke-Bridge 'daedalus_read' ([ordered]@{ symbol = 'DEVBRIDGE_SYMBOL_DOES_NOT_EXIST' }) -AllowError
Assert-True ($missingSymbol.error_code -eq 'configured_symbol_missing') 'Unknown Daedalus symbol was not rejected cleanly.'
Record-Pass 'Daedalus unknown-symbol validation'

$monitor = Invoke-Bridge 'monitor' ([ordered]@{ target_type = 'player'; duration_ms = 400; interval_ms = 100; max_samples = 8 })
Assert-True ($monitor.sample_count -ge 2 -and $monitor.samples.Count -eq $monitor.sample_count) `
    'Player monitor did not collect multiple samples.'
Record-Pass 'bounded player state monitor'

$cancelled = Invoke-Bridge 'monitor' ([ordered]@{ target_type = 'player'; duration_ms = 15000; interval_ms = 100; max_samples = 150 }) -AllowError -ClientTimeoutSeconds 2
Assert-True ($cancelled.error_code -eq 'timeout') 'A short client deadline did not cancel a long monitor operation.'
Assert-True (-not (Test-Path -LiteralPath (Join-Path $exchangeRoot 'request.json'))) 'The cancelled monitor left an active request file.'
$pingAfterCancel = Invoke-Bridge 'ping'
Assert-True ($pingAfterCancel.ok -eq $true) 'The plugin did not accept a new request after monitor cancellation.'
Record-Pass 'long-monitor timeout cancellation and request cleanup'

$screenshot = Invoke-Bridge 'screenshot'
$screenshotPath = [IO.Path]::GetFullPath($screenshot.path)

if (-not $screenshotPath.StartsWith($exchangeRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Screenshot path escaped the testbed exchange directory.'
}
Assert-True ((Test-Path -LiteralPath $screenshotPath -PathType Leaf) -and (Get-Item -LiteralPath $screenshotPath).Length -gt 0) `
    'Screenshot output file is missing or empty.'
Remove-Item -LiteralPath $screenshotPath -Force
Record-Pass 'post-render screenshot and local output cleanup'

if ($IncludePlayerControl) {
    Assert-True ($ping.capabilities.mutating.player_control -eq $true) `
        'Player-control runtime checks require allow_player_control=true in the isolated testbed config.'
    $playerBefore = Invoke-Bridge 'player_state'
    $turn = Invoke-Bridge 'player_turn' ([ordered]@{ look_at = @(
        [double]$playerBefore.position[0], [double]$playerBefore.position[1],
        ([double]$playerBefore.position[2] + 1000.0)) })
Assert-True ($null -ne $turn.heading) 'Player heading change returned no resulting heading.'
    $teleport = Invoke-Bridge 'player_teleport' ([ordered]@{ position = @(
        [double]$playerBefore.position[0], [double]$playerBefore.position[1], [double]$playerBefore.position[2]) })
    Assert-True ($null -ne $teleport.actual_position) 'Player teleport returned no actual position.'
    $walk = Invoke-Bridge 'player_walk_to' ([ordered]@{ target = @(
        [double]$playerBefore.position[0], [double]$playerBefore.position[1], [double]$playerBefore.position[2]); duration_ms = 2000 })
    Assert-True ($walk.reason -eq 'target_reached') "No-op walk-to did not safely complete: $($walk.reason)"
    $move = Invoke-Bridge 'player_move' ([ordered]@{ direction = 'forward'; duration_ms = 300 })
    Assert-True ($move.reason -eq 'duration_elapsed' -and $move.input_released -eq $true) `
        'Timed player movement did not finish and restore its input state.'
    Record-Pass 'gated player turn, teleport, walk-to and timed input cleanup'
}

"Completed $($passed.Count) runtime checks in $GameRoot"
