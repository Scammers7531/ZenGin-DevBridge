[🇵🇱 Polski](API.md) | [🇬🇧 English](API.en.md)

# DevBridge file API

Protocol version: **1**. This is a local, single-request-at-a-time file protocol. The plugin polls from the Gothic render hook; it does not open a network port.

## Exchange files

The default exchange folder is <game-root>\ZenGinDevBridge. A local configuration may choose a different single folder name, relative to the game root.

| File | Purpose |
| --- | --- |
| request.json | One UTF-8 request object, written atomically by the client. |
| response.json | The matching response object. Clients must compare id. |
| cancel.<id> | Empty marker telling the plugin to stop work for that request. |
| plugin-heartbeat.json | Plugin version, protocol version, hook status, and configuration validity. |
| diagnostics.jsonl | Append-only error codes with request ID/command when available; no executable or working-directory paths. |
| client.lock | Created exclusively by the bundled PowerShell client and contains its owner ID. |

Send one request at a time. The plugin validates the request ID before using it in a cancellation filename. Requests larger than 64 KiB are rejected. The plugin publishes responses by replacing a temporary file atomically. If response publication fails, it retries without rerunning the command.

`client.lock` coordinates only concurrent runs of the bundled PowerShell client. A client removes the file only if it created it and the file still contains its own ID. The plugin and external protocol clients do not use this lock, so they must serialize their requests themselves. If a client crashes and leaves a lock file behind, verify that no DevBridge client is active before removing it manually.

### PowerShell client

The included client runs in Windows PowerShell 5.1 and PowerShell 7; the test scripts require PowerShell 7 (pwsh). -SaveJsonPath saves the full JSON response. -ExportCsvPath exports results rows only for raycast_batch and surface_grid.

    $client = '.\client\powershell\Query-DevBridge.ps1'
    $gameRoot = 'C:\Games\Gothic II'
    $payload = @{ default_flags = 1058; probes = @(@{ start = @(100, 1000, 200); direction = @(0, -1500, 0) }) } | ConvertTo-Json -Depth 4 -Compress
    & $client -GameRoot $gameRoot -Command raycast_batch -PayloadJson $payload -SaveJsonPath '.\raycasts.json' -ExportCsvPath '.\raycasts.csv'

## Request envelope

    {
      "id": "agent-0001",
      "command": "ping",
      "protocol_version": 1,
      "expires_at_unix_ms": 1795700000000
    }

- id: required, 1–80 ASCII letters, digits, period, underscore, or hyphen; period and double-period are invalid.
- command: required; 1–64 UTF-8 bytes with no embedded NUL. An unknown but syntactically valid name receives an unknown_command response.
- protocol_version: optional for older clients; if absent, the request is interpreted as version 1. Other versions receive protocol_version_unsupported.
- expires_at_unix_ms: recommended absolute Unix time in milliseconds. The maximum accepted lifetime is 180 seconds. Older clients may omit it; for those requests, the plugin uses the request-file timestamp and a 120 second maximum age.

When the deadline expires, the plugin returns `timeout` unless it observes a cancellation marker first. A client may create `cancel.<id>`; the plugin checks it at safe checkpoints, including between frames during multi-frame scans. If cancellation and expiry are observed in the same check, `cancelled` takes precedence. Cancellation does not interrupt an engine call already in progress. `camera_restore` ignores cancellation markers, but the request still needs a valid envelope and an unexpired deadline.

After its own timeout, the bundled client writes a marker only if `request.json` still contains its ID, then waits up to three seconds for a response. It does not delete a pending request. If no acknowledgment arrives, it exits with an error and leaves the file state for inspection; do not send another request until the previous one has been processed or manually resolved.

## Response envelope and errors

Every response contains id, command, protocol_version, plugin_version, and ok. Error responses also contain:

    {
      "ok": false,
      "error_code": "world_not_ready",
      "error": "no world is currently loaded"
    }

The legacy error string is preserved. Clients should branch on error_code.

| Error code | Meaning |
| --- | --- |
| invalid_request | Invalid request field, type, range, vector, flag, or deadline. Used after a valid envelope is read; some structural failures receive no response, as described below. |
| protocol_version_unsupported | The requested protocol version is unsupported; the response includes `supported_protocol_versions`. |
| request_file_error | The plugin cannot read the request file timestamp for a legacy request without `expires_at_unix_ms`. |
| timeout | The request deadline passed before processing started or completed. |
| cancelled | The plugin observed `cancel.<id>` at a checkpoint before expiry. |
| world_not_ready | The game world or required parser is unavailable. |
| world_changed | The world changed during a multi-frame scan; partial results were discarded. |
| capability_disabled | A state-changing or configuration-backed command is disabled or unconfigured. |
| unknown_command | The command name is unsupported. |
| unsupported_scenario | The scenario name is not present in the local `scenario_dispatch.scenarios` map. |
| scan_failed | General `raycast_batch` or `surface_grid` execution failure; details are in `error`. |
| parser_unavailable | The Daedalus parser is unavailable while handling a scenario command. |
| configured_symbol_missing | The configured symbol name was not found in the Daedalus parser. |
| configured_symbol_unavailable | The parser found the symbol index but did not return an available symbol object. |
| configured_symbol_type_mismatch | A configured symbol is not a scalar Daedalus integer variable. |
| configured_symbol_not_writable | The scenario queue symbol is constant and cannot be written. |
| scenario_queue_busy | The configured queue symbol is nonzero, so the previous request has not been consumed. |
| camera_unavailable | The camera or its VOb is unavailable. |
| target_vob_not_found | The VOb named by `target_vob` in `camera_set` was not found. |
| internal_error | Unexpected plugin exception; check `diagnostics.jsonl`. |

### Rejections without `response.json`

The plugin publishes a response only when it can read a valid JSON object with a safe `id` and `command`. Invalid JSON or missing/non-string `id`/`command` fields are logged as `malformed_request`. An invalid `id`, or an empty `command`, a command longer than 64 UTF-8 bytes, or one containing NUL is logged as `invalid_request_identity`. In either case the request is rejected without a response. An unsupported command name that meets the syntax limits returns `unknown_command`. A file larger than 64 KiB is logged as `request_too_large` and removed.

Exchange-folder or request-file access failures (`exchange_directory_unavailable`, `request_file_unavailable`, `request_open_failed`) and response publication failures (`response_write_failed`, `response_request_mismatch`) can also leave no new response. When publication fails, the plugin retries if it is still handling the same request. Always match the `id` in `response.json` and inspect `diagnostics.jsonl` when something goes wrong; an older response file does not confirm the current request.

### Diagnostic codes in `diagnostics.jsonl`

Every error code returned in `error_code` is also written to diagnostics. The following codes concern the file layer, render hook, or heartbeat and are not protocol responses by themselves:

| Diagnostic code | Meaning |
| --- | --- |
| exchange_directory_unavailable | The plugin cannot create or access the exchange folder. |
| request_file_unavailable | The request file cannot be inspected. |
| request_open_failed | The request file cannot be opened; the plugin may retry on its next poll. |
| request_too_large | The request exceeds 64 KiB; the file is removed without a response. |
| malformed_request | The JSON or envelope lacks valid string `id` and `command` fields; the file is rejected without a response. |
| invalid_request_identity | `id` is invalid, or `command` is empty, longer than 64 UTF-8 bytes, or contains NUL; the file is rejected without a response. |
| protocol_version_unsupported | The protocol version is unsupported; in addition to the diagnostic, the client receives an `error_code` response. |
| response_write_failed | Response publication failed; the plugin retries for the active request. |
| response_request_mismatch | `request.json` no longer contains the active `id`; the plugin does not publish a stale response. |
| heartbeat_write_failed | The temporary heartbeat file could not be written. |
| heartbeat_publish_failed | The heartbeat file could not be published. |
| heartbeat_error | An unexpected exception occurred while writing the heartbeat. |
| render_hook_processing_error | An unexpected exception escaped per-request handling in the render hook. |

## Capabilities

ping returns plugin and protocol versions, world_ready, and capabilities. read_only lists observation commands; mutating reports whether state-changing operations are available. A true value means a capability is enabled or configured; it does not guarantee that the world, camera, or target is available when called. player_control in the mutating group is an umbrella flag, not a command.

Example response with default configuration:

    {
      "plugin": "ZenGinDevBridge",
      "plugin_version": "0.2.0",
      "protocol_version": 1,
      "world_ready": true,
      "capabilities": {
        "read_only": {
          "ping": true,
          "vob": true,
          "raycast": true,
          "raycast_batch": true,
          "surface_grid": true,
          "camera_get": true,
          "vob_search": true,
          "vob_hierarchy": true,
          "npc_search": true,
          "npc_inspect": true,
          "player_state": true,
          "player_collision": true,
          "waypoint_search": true,
          "waypoint_inspect": true,
          "waynet_local_graph": true,
          "waynet_nearest": true,
          "waynet_check": true,
          "waynet_route": true,
          "daedalus_search": true,
          "daedalus_read": true,
          "mover_inspect": true,
          "monitor": true,
          "screenshot": true,
          "scenario_status": false
        },
        "mutating": {
          "camera_set": false,
          "scenario_start": false,
          "player_teleport": false,
          "player_turn": false,
          "player_move": false,
          "player_walk_to": false,
          "player_control": false,
          "camera_restore": true
        },
        "config_valid": true,
        "camera_control_enabled": false,
        "player_control_enabled": false,
        "scenario_dispatch_enabled": false
      }
    }

## Commands

### ping

No additional fields. Returns plugin identity/version, world_ready, stage (retained for older clients), and capabilities.

### vob

    {"command":"vob","name":"PC_HERO"}

Returns found; when found, also returns name, class, matrix, bbox3d_min, bbox3d_max, collision flags, and collision_object_present.

### raycast

    {"command":"raycast","start":[100,1000,200],"direction":[0,-1500,0],"flags":1058}

start and direction are finite [x,y,z] vectors with absolute components no larger than 10,000,000. flags is an integer from 0 to 32,767. The result has hit, ray length, hit position/height/normal, geometry kind, and material/VOb data when available.

Coordinates are Gothic world positions in raw engine units; the plugin does not convert them to meters. X and Z are horizontal axes, and Y is vertical. direction is a displacement vector, not a unit vector: the ray endpoint is start + direction and its length is the vector's magnitude. flags is a bitmask passed to zCWorld::TraceRayNearestHit:

| Value | Flag |
| ---: | --- |
| 1 | zTRACERAY_VOB_IGNORE_NO_CD_DYN |
| 2 | zTRACERAY_VOB_IGNORE |
| 4 | zTRACERAY_VOB_BBOX |
| 8 | zTRACERAY_VOB_OBB |
| 16 | zTRACERAY_STAT_IGNORE |
| 32 | zTRACERAY_STAT_POLY |
| 64 | zTRACERAY_STAT_PORTALS |
| 128 | zTRACERAY_POLY_NORMAL |
| 256 | zTRACERAY_POLY_IGNORE_TRANSP |
| 512 | zTRACERAY_POLY_TEST_WATER |
| 1024 | zTRACERAY_POLY_2SIDED |
| 2048 | zTRACERAY_VOB_IGNORE_CHARACTER |
| 4096 | zTRACERAY_FIRSTHIT |
| 8192 | zTRACERAY_VOB_TEST_HELPER_VISUALS |
| 16384 | zTRACERAY_VOB_IGNORE_PROJECTILES |

The example value 1058 combines zTRACERAY_VOB_IGNORE (2), zTRACERAY_STAT_POLY (32), and zTRACERAY_POLY_2SIDED (1024).

### raycast_batch

    {
      "command":"raycast_batch",
      "default_flags":1058,
      "probes":[
        {"start":[100,1000,200],"direction":[0,-1500,0]},
        {"start":[120,1000,200],"direction":[0,-1500,0],"flags":1058}
      ]
    }

Accepts 1–256 probes. The plugin traces at most 16 probes per frame and returns ordered results after completion.

### surface_grid

    {
      "command":"surface_grid",
      "center":[100,1000,200],
      "radius_x":20,
      "radius_z":20,
      "spacing":20,
      "down_range":2000,
      "include_bbox":false
    }

Radii are 0–5,000, spacing is 10–5,000, down range is 10–100,000, and the generated grid is limited to 121 samples. The plugin processes up to 3 samples per frame. Each row compares visual/static-world/dynamic-VOb raycasts and player floor/water/ceiling support. include_bbox adds a VOb bounding-box trace.

`surface_grid` and `character_support` provide geometry measurements and a current read of the player's collision object. They do not simulate character walking: they do not predict paths, stair traversal, jumps, animation, or AI reactions. Use them to diagnose surfaces and collisions, not as a complete navigation or walkability test.

### camera_get

No additional fields. Reads camera transforms, camera VOb data, and camera AI mode/flags when available.

### camera_set — state-changing, disabled by default

    {"command":"camera_set","position":[100,300,400],"look_at":[0,100,0]}

Provide either look_at as a finite vector or target_vob as a VOb name. The first change saves the original transform and camera AI flags. Further updates keep that original restore point. The plugin applies the transform only while the same camera and world remain current. Call camera_restore when the camera task is complete.

### camera_restore — recovery command

No additional fields and available even when camera control is disabled. The request still needs a valid envelope and an unexpired deadline. The response reports restored, camera_ai_restored, and world_matched. The transform is restored only if the same saved world, camera, and camera VOb are still current. Camera AI flags are restored separately if the original AI object is still present. If there is no active override or the objects changed/became unavailable, the corresponding fields are false; the saved restore point is then cleared and cannot be retried. A world change discards the saved point safely without dereferencing stale pointers.

### scenario_start — state-changing, disabled by default

    {"command":"scenario_start","scenario":"example_step"}

Dispatch is available only when enabled and configured locally. The plugin writes the configured integer into the configured Daedalus symbol; it never calls a parser function from the render hook. The integer must be in the range 1–32,767. The scenario name is a local map key and is not built into the public plugin.

The plugin remembers the last dispatched scenario name/ID pair in the current world. Sending that same pair again returns `ok: true` and `already_dispatched: true`, but does not write it to the queue again—even if the mod has already cleared the queue symbol. A new dispatch is allowed after another scenario is successfully queued or after the world changes. This guard prevents accidental duplicates but means that retrying the same scenario in one world needs a planned reset through a different scenario or a world change.

### scenario_status — configuration-backed read

No additional fields. Reads configured Daedalus integer symbols. The response includes a scenario_status object plus flat aliases matching the locally configured keys, which lets existing environment-specific test clients keep their expected response fields.

## Additional diagnostic commands — v0.2.0 candidate

This section describes the local, unpublished candidate. It is not a public release yet. The protocol remains at version 1; existing commands keep their request formats.

### VOb search

    {"command":"vob_search","name_contains":"CHEST","class_contains":"zCVob","center":[0,0,0],"radius":5000,"sort_by":"distance","limit":25,"offset":0,"scan_limit":20000}

Optional filters are name_exact, name_contains, and class_contains. Set the reference point with center or center_vob, never both. radius (0–100000) requires a reference point; sort_by supports only distance. limit is 1–100, offset 0–100000, and scan_limit 1–200000. A single request has an internal 15 ms scan budget. The response contains results, nodes_scanned, total_vobs, scan_truncated, and scan_time_limited. offset pages matches from the current scan; the API does not provide a cursor to resume a scan interrupted by the time limit.

### VOb hierarchy

Empty ancestors and children arrays mean that the current world returned no parent or child links for a valid target node. This is not a request error or proof that the entire world has no hierarchy.

    {"command":"vob_hierarchy","name":"PC_HERO","children_depth":3,"max_nodes":128}

name identifies the exact VOb. children_depth is 0–8 (default 2), and max_nodes is 1–256 (default 64). The response contains target, ancestors, children, children_nodes_scanned, and children_truncated. The number of ancestors is capped at 64.

### NPC search and inspection

    {"command":"npc_search","name_contains":"GUARD","instance_contains":"MIL","guild_contains":"GUARD","center_vob":"PC_HERO","radius":5000,"limit":25,"offset":0,"scan_limit":20000}

npc_search filters by name_contains, instance_contains, and guild_contains. center or center_vob sorts results by distance; radius (0–100000) requires one of those fields. limit is 1–100, offset 0–100000, scan_limit 1–200000; filters are limited to 128 characters. A scan has a 15 ms time budget. Partial scans are reported through scan_truncated and scan_time_limited.

    {"command":"npc_inspect","name":"NPC_VOB_NAME","inventory_limit":8}

npc_inspect takes the VOb name of an NPC and inventory_limit 0–16 (default 8). It returns available information about its instance, position and orientation, health, life state, guild, AI, routine and current action, movement target, combat, waypoint, animation, dialogue, and inventory. Optional data is omitted or marked unavailable; the plugin does not infer it.

### Player state and collision diagnostics

player_state takes no additional fields. It returns the same information groups as npc_inspect for the player, the player flag, available oCAIPlayer data (movement, velocity, foot and head height, ground contact, falling, and sliding), collision_support, and a helper ground_probe.

player_collision takes no additional fields. It returns character_support, four obstacle raycasts: forward and backward for 180 units, left and right for 140 units, plus available movement and collision state. full_character_simulation is false in the response: these are engine reads and probes, not a full collision simulation or proof that the character can traverse a route.

### WayNet

    {"command":"waypoint_search","name_contains":"WP","limit":64,"offset":0}

waypoint_search accepts name_contains and/or exact name; limit is 1–256, offset is 0–100000. waypoint_inspect requires an exact name. Both responses describe the waypoint position and its available connections.

    {"command":"waynet_nearest","position":[0,0,0]}

Alternatively, waynet_nearest accepts a VOb name in vob. waynet_local_graph requires center (a waypoint name) and optionally accepts depth 0–8 (default 2) and max_nodes 1–256 (default 64). waynet_check accepts component_scan_limit 1–100000 (default 100000); it reports waypoint and edge counts, isolated points, dangling edges, and a sample of weakly connected components. Sample lists are bounded.

    {"command":"waynet_route","start":"WP_START","target":"WP_END","max_nodes":20000}

waynet_route runs bounded Dijkstra over WayNet edges. It uses a positive edge cost, or Euclidean distance between points when no cost is available. max_nodes is 1–100000. The response explicitly reports algorithm and physical_walkability_verified:false. A graph route is neither the AI's navigation result nor a guarantee that the character can traverse the world.

### Daedalus reads and mover inspection

daedalus_read can also read constants. Class variables return metadata only with value_available:false because they require an active instance context. Array reads are limited to the range selected by start_index and count.

    {"command":"daedalus_search","name_contains":"TEST","limit":64,"offset":0,"scan_limit":20000}

daedalus_search returns symbol metadata; limit is 1–256, offset 0–100000, scan_limit 1–100000, and the filter is limited to 128 characters. The scan is capped at 15 ms. daedalus_read requires symbol and optionally accepts start_index 0–4095 and count 1–256. Reads support int, float, and string, including arrays within the requested range. Strings are truncated after 4096 bytes. Unsupported types and class variables return value_available:false with value_reason. The plugin does not write arbitrary Daedalus symbols.

    {"command":"mover_inspect","name":"MOVER_VOB_NAME"}

mover_inspect only reads zCMover state, movement parameters, position, and up to 64 keyframes. related_target is informational only. trigger_invoked is always false; this command does not activate the mover.

### Player control — opt-in

Movement uses an oCAIHuman::DoAI hook, not direct position changes or a custom physics simulation. The hook injects input only for the current living player's AI in the same world. Immediately before the original DoAI call, it sets the selected key, increments control_ai_frames, calls the original AI exactly once, and restores the key state that applied during that frame. The operation saves the previous input state when it begins; when it ends, the plugin restores that state, clears the active operation, and returns input_released:true.

control_ai_frames only confirms that input reached the hook. distance_moved is the actual 3D Euclidean distance in raw Gothic world units. player_walk_to sets a normalized horizontal heading once with SetHeadingAtWorld, then emulates forward input through normal AI processing. It does not use pathfinding, avoid obstacles, or simulate physics or animation.

At render-hook checkpoints, cancellation is checked before the overall request deadline; if both are observed together, cancelled takes precedence. Expiration of the player_move duration returns duration_elapsed even at zero distance. player_walk_to returns target_reached when within tolerance, movement_stalled when progress stops, or movement_timeout when its operation duration expires before arrival. Expiration of the overall request deadline during control also ends the operation as movement_timeout. Cancellation, expiration, world change, player loss, and errors restore input and clear the active control operation.

player_teleport, player_turn, player_move, and player_walk_to are disabled by default. They require allow_player_control:true in local configuration and a ping result with capabilities.mutating.player_control:true. The player_control field in capabilities.mutating is an umbrella availability flag, not a command name.

    {"command":"player_teleport","waypoint":"WP_TARGET","look_at":[0,0,0]}

player_teleport requires position or waypoint; look_at is optional. It returns the previous, requested, and actual positions. It uses SetPositionWorld; the engine revalidates collision on its next movement frame. Teleporting is not a walking test.

    {"command":"player_turn","look_at":[100,0,200]}

player_turn changes the player's horizontal heading. look_at must differ from the player's current position horizontally.

    {"command":"player_move","direction":"forward","duration_ms":1000}

player_move accepts direction: forward, backward, strafe_left, or strafe_right and duration_ms 100–30000 (default 1000). It injects the player's input immediately before that player's AI is processed, then restores the previous key state when it ends. The response includes start and end positions, distance_moved, elapsed_ms, input_released, world_matched, and control_ai_frames—the number of AI frames in which the plugin supplied input. In scripted scenes or when movement is blocked, the command may finish without moving the player; check distance_moved instead of assuming movement from the reason alone.

    {"command":"player_walk_to","waypoint":"WP_TARGET","duration_ms":5000,"tolerance":100,"stagnation_ms":2500}

player_walk_to requires target coordinates or a waypoint name. It accepts duration_ms 100–30000 (default 5000), tolerance 20–1000 (default 100), and stagnation_ms 500–10000 (default 2500). It sets the heading and emulates forward input for the duration of the operation. It does not compute obstacle-avoiding paths. The response includes positions, distance, target_distance, target_reached, a completion reason, input_released, and control_ai_frames. Lack of progress ends the operation with movement_stalled. On timeout, cancellation, world change, or player loss, the plugin releases the input.

### Screenshot and monitoring

screenshot takes no additional fields. After rendering the current frame it saves a 32-bit TGA in the exchange directory using a generated safe name, devbridge-<id>.tga. Existing files are not overwritten. The response returns filename, path, format, bytes, and capture_stage. The plugin supplies the image; the client is responsible for interpreting it.

    {"command":"monitor","target_type":"player","duration_ms":5000,"interval_ms":250,"max_samples":20,"stagnation_ms":1000,"reference_vob":"NPC_VOB_NAME"}

target_type is player, npc, or mover; npc and mover require target with a VOb name. Optional reference_vob adds reference_distance. duration_ms is 100–30000, interval_ms 50–5000, max_samples 1–256, and stagnation_ms 0–10000. The monitor returns position samples and, depending on the target, NPC, movement, or mover keyframe data. It ends on elapsed time, sample limit, or no-progress detection. The request remains subject to its deadline and cancel.<id> marker; timeout/cancellation returns an error and clears the active request. Only one monitor or player-control operation can run at a time.

### Additional error codes

| Error code | Meaning |
| --- | --- |
| vob_search_unavailable | VOb class metadata is unavailable. |
| vob_not_found | The requested VOb or reference point was not found. |
| vob_hierarchy_unavailable | The VOb tree node is unavailable or changed. |
| npc_search_unavailable | NPC class metadata is unavailable. |
| npc_not_found | The NPC was not found. |
| object_not_npc | The requested VOb is not an NPC. |
| player_unavailable | The player is unavailable, dead, or unconscious. |
| waynet_unavailable | The current world's WayNet is unavailable. |
| waypoint_not_found | The waypoint was not found. |
| object_not_mover | The requested VOb is not a zCMover. |
| input_unavailable | The game input system is unavailable for player control. |
| operation_busy | Another monitoring or player-control operation is active. |
| target_not_found | The monitored target disappeared. |
| reference_not_found | The monitor reference object disappeared. |
| player_changed | The active player object changed during monitoring. |
| movement_stalled | The player's position made no required progress; input was released. |
| movement_timeout | The walk attempt did not reach its target before its deadline. |
| movement_failed | An unexpected movement error occurred; input was released. |
| monitor_failed | An unexpected monitoring error occurred. |
| renderer_unavailable | The renderer is unavailable for a screenshot. |
| screenshot_directory_unavailable | The screenshot output directory could not be created. |
| screenshot_exists | The generated screenshot path already exists; the file is not overwritten. |
| screenshot_capture_failed | The frame buffer could not be copied or prepared. |
| screenshot_save_failed | A valid, non-empty TGA could not be written. |
| screenshot_failed | An unexpected screenshot error occurred. |
## Local configuration

scenario_dispatch.request_symbol must map to a writable scalar global int variable. Each status_symbols entry must be a scalar global int; a constant is allowed for read-only status, but not for the queue. Class variables, arrays, and non-int types are rejected before GetValue or SetValue with configured_symbol_type_mismatch. A constant queue returns configured_symbol_not_writable. daedalus_read can read constants but cannot read class-variable values without an instance context.

Create <game-root>\ZenGinDevBridge.config.json only when you need a non-default exchange folder or state-changing command. The file is limited to 16 KiB and supports:

    {
      "config_version": 1,
      "exchange_directory": "ZenGinDevBridge",
      "allow_camera_control": false,
      "allow_scenario_start": false,
      "allow_player_control": false,
      "scenario_dispatch": {
        "request_symbol": "BRIDGE_SCENARIO_REQUEST",
        "scenarios": {"example_step": 1},
        "status_symbols": {"queue_value": "BRIDGE_QUEUE_VALUE"}
      }
    }

exchange_directory must be one folder name under the game root; absolute paths and parent traversal are rejected. Camera and scenario changes default to disabled. Scenario names, IDs, and Daedalus symbols belong in this local file; do not commit environment-specific mappings. If the file cannot be parsed or validated, mutating capabilities are disabled and the reason is reported by ping/heartbeat.

The plugin loads and caches configuration when its render hook first runs after game startup. Changes to the file are not applied to an already running process; exit and restart the game after every edit. Restarting only the PowerShell client is not sufficient.

When `scenario_start` needs to enqueue a new request, the plugin resolves the configured queue symbol in the parser and checks that it is a scalar integer variable (`zPAR_TYPE_INT`, one element) before reading or writing it. On every `scenario_status` read, the plugin checks each configured status symbol in the same way. The queue `request_symbol` must also be writable, meaning it cannot be a constant. A wrong type or array returns `configured_symbol_type_mismatch`; a constant queue returns `configured_symbol_not_writable`. `ping` confirms that the configuration file and mappings are valid, but it does not confirm that the symbols exist or have the right type in the currently loaded parser.

## Agent usage pattern

1. Call ping and inspect protocol_version, world_ready, and capabilities.
2. Use vob, raycast, raycast_batch, or surface_grid for observation.
3. Before camera_set, scenario_start, or a player_* command, check that the corresponding capability is true and obtain explicit authorization from the user or calling system.
4. Use a unique request ID, include an expiry, and wait for the response with that ID.
5. On client timeout, write cancel.<id> and wait for the cancellation response before sending another request.
6. After camera movement, call camera_restore and check its restored result.

For a current expiry value, calculate the Unix time in milliseconds at request creation and add the chosen timeout duration. Do not reuse the example timestamp from this document.
