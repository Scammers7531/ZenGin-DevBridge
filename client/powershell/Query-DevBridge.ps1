[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][ValidateSet('ping','vob','raycast','raycast_batch','surface_grid','scenario_start','scenario_status','camera_get','camera_set','camera_restore','vob_search','vob_hierarchy','npc_search','npc_inspect','player_state','player_collision','waypoint_search','waypoint_inspect','waynet_nearest','waynet_local_graph','waynet_check','waynet_route','daedalus_search','daedalus_read','mover_inspect','player_teleport','player_turn','player_move','player_walk_to','screenshot','monitor')][string]$Command,
    [string]$Name,
    [double[]]$Start,
    [double[]]$Direction,
    [int]$Flags=0,
    [string]$PayloadJson,
    [string]$SaveJsonPath,
    [string]$ExportCsvPath,
    [ValidateRange(1,120)][int]$TimeoutSeconds=15,
    [string]$ExchangeDirectory,
    [string]$GameRoot
)
$ErrorActionPreference='Stop'
if(-not $ExchangeDirectory){
    if(-not $GameRoot){
        $candidate=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\..\..'))
        if(Test-Path -LiteralPath (Join-Path $candidate 'System\Gothic2.exe')){$GameRoot=$candidate}
        else{throw 'Provide -GameRoot or -ExchangeDirectory; the game root could not be inferred.'}
    }
    $GameRoot=[IO.Path]::GetFullPath($GameRoot)
    $exchangeName='ZenGinDevBridge'
    $configPath=Join-Path $GameRoot 'ZenGinDevBridge.config.json'
    if(Test-Path -LiteralPath $configPath){
        $config=Get-Content -LiteralPath $configPath -Raw -Encoding UTF8|ConvertFrom-Json
        if($config.exchange_directory){$exchangeName=[string]$config.exchange_directory}
    }
    if([string]::IsNullOrWhiteSpace($exchangeName) -or $exchangeName -in @('.','..') -or $exchangeName -match '[/\\:]'){
        throw 'exchange_directory in local configuration must be one folder name.'
    }
    $ExchangeDirectory=Join-Path $GameRoot $exchangeName
}
[void][IO.Directory]::CreateDirectory($ExchangeDirectory)
if($Command -eq 'vob' -and [string]::IsNullOrWhiteSpace($Name)){throw '-Name is required for vob'}
if($Command -eq 'raycast' -and ($Start.Count -ne 3 -or $Direction.Count -ne 3)){throw 'Raycast requires three values in -Start and -Direction'}

$id=[Guid]::NewGuid().ToString('N')
$lockPath=Join-Path $ExchangeDirectory 'client.lock'
$requestPath=Join-Path $ExchangeDirectory 'request.json'
$tempPath=Join-Path $ExchangeDirectory "request.$id.tmp"
$responsePath=Join-Path $ExchangeDirectory 'response.json'
$lockStream=$null
$lockAcquired=$false
function Add-HitColumns([System.Collections.IDictionary]$Row,[string]$Prefix,$Hit){
    $Row[$Prefix+'_hit']=[bool]$Hit.hit
    $Row[$Prefix+'_kind']=$Hit.kind
    $Row[$Prefix+'_height']=$Hit.height
    $Row[$Prefix+'_name']=$Hit.vob_name
    $Row[$Prefix+'_class']=$Hit.vob_class
    if($Hit.position){$Row[$Prefix+'_x']=$Hit.position[0];$Row[$Prefix+'_y']=$Hit.position[1];$Row[$Prefix+'_z']=$Hit.position[2]}
    if($Hit.normal){$Row[$Prefix+'_normal']=ConvertTo-Json -InputObject @($Hit.normal) -Compress}
    if($Hit.material){$Row[$Prefix+'_material']=$Hit.material.name;$Row[$Prefix+'_no_coll_det']=$Hit.material.no_coll_det}
}
try{
    $lockStream=[IO.File]::Open($lockPath,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None)
    $lockAcquired=$true
    $lockBytes=[Text.Encoding]::ASCII.GetBytes($id)
    $lockStream.Write($lockBytes,0,$lockBytes.Length)
    $lockStream.Dispose();$lockStream=$null
    if(Test-Path -LiteralPath $requestPath){throw 'Unprocessed request exists and was preserved'}
    $deadline=[DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    $request=[ordered]@{
        command=$Command
        id=$id
        protocol_version=1
        expires_at_unix_ms=([DateTimeOffset]$deadline).ToUnixTimeMilliseconds()
    }
    if($Command -eq 'vob'){$request.name=$Name}
    if($Command -eq 'raycast'){$request.start=@($Start);$request.direction=@($Direction);$request.flags=$Flags}
    if($PayloadJson){
        $payload=ConvertFrom-Json -InputObject $PayloadJson
        if($null -eq $payload -or $payload -is [string] -or $payload -is [array]){throw '-PayloadJson must be a JSON object'}
        foreach($property in $payload.PSObject.Properties){
            if($property.Name -in @('id','command')){throw 'Payload cannot replace command or id'}
            $request[$property.Name]=$property.Value
        }
    }
    $json=$request|ConvertTo-Json -Compress -Depth 16
    $encoding=[Text.UTF8Encoding]::new($false)
    [IO.File]::WriteAllText($tempPath,$json,$encoding)
    [IO.File]::Move($tempPath,$requestPath)
    $cancelPath=Join-Path $ExchangeDirectory "cancel.$id"
    $cancelSent=$false
    $cancelDeadline=$null
    while($true){
        if(Test-Path -LiteralPath $responsePath){
            try{
                $response=Get-Content -LiteralPath $responsePath -Raw -Encoding UTF8|ConvertFrom-Json
                if($response.id -eq $id){
                    $responseJson=ConvertTo-Json -InputObject $response -Depth 20
                    if($SaveJsonPath){[IO.File]::WriteAllText([IO.Path]::GetFullPath($SaveJsonPath),$responseJson,[Text.UTF8Encoding]::new($false))}
                    if($ExportCsvPath){
                        $rows=@()
                        foreach($item in $response.results){
                            $row=[ordered]@{index=$item.index}
                            if($response.command -eq 'surface_grid'){
                                $row.x=$item.x;$row.z=$item.z;$row.ray_start_y=$item.ray_start_y
                                Add-HitColumns $row 'visual' $item.visual
                                Add-HitColumns $row 'static_world' $item.static_world
                                Add-HitColumns $row 'vob_cd_dynamic' $item.vob_cd_dynamic
                                if($item.vob_bbox){Add-HitColumns $row 'vob_bbox' $item.vob_bbox}
                                $support=$item.character_support
                                $row.support_available=$support.available;$row.supported=$support.supported
                                $row.floor_y=$support.floor_y;$row.water_y=$support.water_y;$row.ceiling_y=$support.ceiling_y
                                $row.floor_is_vob=$support.floor_is_vob;$row.floor_is_stair=$support.floor_is_stair
                                if($support.floor_material){$row.floor_material=$support.floor_material.name;$row.floor_no_coll_det=$support.floor_material.no_coll_det}
                            }else{Add-HitColumns $row 'ray' $item}
                            $rows+=([pscustomobject]$row)
                        }
                        $csvParent=Split-Path -Parent ([IO.Path]::GetFullPath($ExportCsvPath))
                        if($csvParent -and -not(Test-Path -LiteralPath $csvParent)){[void][IO.Directory]::CreateDirectory($csvParent)}
                        $rows|Export-Csv -LiteralPath ([IO.Path]::GetFullPath($ExportCsvPath)) -NoTypeInformation -Encoding UTF8
                    }
                    Write-Output $responseJson
                    if(-not $response.ok){exit 2}
                    exit 0
                }
            }catch{}
        }
        $now=[DateTime]::UtcNow
        if(-not $cancelSent -and $now -ge $deadline){
            if(Test-Path -LiteralPath $requestPath){
                try{
                    $pending=Get-Content -LiteralPath $requestPath -Raw -Encoding UTF8|ConvertFrom-Json
                    if($pending.id -eq $id){[IO.File]::WriteAllText($cancelPath,'',[Text.UTF8Encoding]::new($false))}
                }catch{}
            }
            $cancelSent=$true
            $cancelDeadline=$now.AddSeconds(3)
        }elseif($cancelSent -and $now -ge $cancelDeadline){
            if(-not (Test-Path -LiteralPath $requestPath) -and (Test-Path -LiteralPath $cancelPath)){
                Remove-Item -LiteralPath $cancelPath -Force -ErrorAction SilentlyContinue
            }
            throw "Timeout after $TimeoutSeconds seconds; cancellation was sent. Check the response and request files in $ExchangeDirectory."
        }
        Start-Sleep -Milliseconds 100
    }
}
finally{
    if($lockStream){$lockStream.Dispose()}
    if(Test-Path -LiteralPath $tempPath){Remove-Item -LiteralPath $tempPath -Force}
    if($lockAcquired -and (Test-Path -LiteralPath $lockPath)){
        try{
            if([IO.File]::ReadAllText($lockPath,[Text.Encoding]::ASCII) -eq $id){
                Remove-Item -LiteralPath $lockPath -Force -ErrorAction Stop
            }
        }catch{}
    }
}
