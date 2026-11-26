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
| client.lock | Used by the bundled PowerShell client to serialize its own requests. |

Send one request at a time. The plugin validates the request ID before using it in a cancellation filename. Requests larger than 64 KiB are rejected. The plugin publishes responses by replacing a temporary file atomically. A response can be retried after a disk error without rerunning the command.

## Request envelope

    {
      "id": "agent-0001",
      "command": "ping",
      "protocol_version": 1,
      "expires_at_unix_ms": 1795700000000
    }

- id: required, 1–80 ASCII letters, digits, period, underscore, or hyphen; period and double-period are invalid.
- command: required, up to 64 characters.
- protocol_version: optional for older clients; if absent, the request is interpreted as version 1. Other versions receive protocol_version_unsupported.
- expires_at_unix_ms: recommended absolute Unix time in milliseconds. The maximum accepted lifetime is 180 seconds. Older clients may omit it; for those requests, the plugin uses the request-file timestamp and a 120 second maximum age.

When a client deadline expires, it should create cancel.<id> and leave the request in place long enough for the plugin to observe cancellation and publish a result. A client must not delete or replace a request while the plugin may be processing it. The bundled client waits up to three seconds for cancellation to be acknowledged.

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
| invalid_request | Invalid JSON fields, types, ranges, vector, flags, or deadline. |
| protocol_version_unsupported | The client requested an unsupported protocol version. |
| timeout | The request expired before completion. |
| cancelled | The client cancelled the request. |
| world_not_ready | No game world is loaded. |
| world_changed | The world changed while a batched scan was running; results were discarded. |
| capability_disabled | A state-changing or configuration-backed command is disabled or unconfigured. |
| unknown_command | The command name is not supported. |
| scan_failed, parser_unavailable, configured_symbol_missing, scenario_queue_busy, camera_unavailable, target_vob_not_found | Command-specific failure; inspect error for details. |
| internal_error | Unexpected plugin error; check diagnostics.jsonl. |

## Capabilities

ping reports the plugin and protocol versions, whether a world is loaded, and capabilities. Capabilities separate read_only commands from mutating commands. A true value means the command is enabled/configured; a world or camera can still be unavailable at call time.

    {
      "plugin": "ZenGinDevBridge",
      "plugin_version": "0.1.0",
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
          "scenario_status": false
        },
        "mutating": {
          "camera_set": false,
          "scenario_start": false,
          "camera_restore": true
        }
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

### camera_get

No additional fields. Reads camera transforms, camera VOb data, and camera AI mode/flags when available.

### camera_set — state-changing, disabled by default

    {"command":"camera_set","position":[100,300,400],"look_at":[0,100,0]}

Provide either look_at as a finite vector or target_vob as a VOb name. The first change saves the original transform and camera AI flags. Further updates keep that original restore point. The plugin applies the transform only while the same camera and world remain current.

### camera_restore — recovery command

No additional fields. Always accepted, including when camera control is disabled. The response reports restored, camera_ai_restored, and world_matched. It does not dereference saved game pointers if the world/camera was replaced or became unavailable.

### scenario_start — state-changing, disabled by default

    {"command":"scenario_start","scenario":"example_step"}

Dispatch is available only when enabled and configured locally. The plugin writes the configured integer into the configured Daedalus symbol; it never calls a parser function from the render hook. The integer must be in the range 1–32,767. The scenario name is a local map key and is not built into the public plugin.

### scenario_status — configuration-backed read

No additional fields. Reads configured Daedalus integer symbols. The response includes a scenario_status object plus flat aliases matching the locally configured keys, which lets existing environment-specific test clients keep their expected response fields.

## Local configuration

Create <game-root>\ZenGinDevBridge.config.json only when you need a non-default exchange folder or state-changing command. The file is limited to 16 KiB and supports:

    {
      "config_version": 1,
      "exchange_directory": "ZenGinDevBridge",
      "allow_camera_control": false,
      "allow_scenario_start": false,
      "scenario_dispatch": {
        "request_symbol": "BRIDGE_SCENARIO_REQUEST",
        "scenarios": {"example_step": 1},
        "status_symbols": {"queue_value": "BRIDGE_QUEUE_VALUE"}
      }
    }

exchange_directory must be one folder name under the game root; absolute paths and parent traversal are rejected. Camera and scenario changes default to disabled. Scenario names, IDs, and Daedalus symbols belong in this local file; do not commit environment-specific mappings. If the file cannot be parsed or validated, mutating capabilities are disabled and the reason is reported by ping/heartbeat.

## Agent usage pattern

1. Call ping and inspect protocol_version, world_ready, and capabilities.
2. Use vob, raycast, raycast_batch, or surface_grid for observation.
3. Before camera_set or scenario_start, verify its mutating capability is true and obtain explicit task-level authorization from the user or calling system.
4. Use a unique request ID, include an expiry, and wait for the response with that ID.
5. On client timeout, write cancel.<id> and wait for the cancellation response before sending another request.
6. After camera movement, call camera_restore and check its restored result.

For a current expiry value, calculate the Unix time in milliseconds at request creation and add the chosen timeout duration. Do not reuse the example timestamp from this document.
