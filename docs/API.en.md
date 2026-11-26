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

The plugin loads and caches configuration when its render hook first runs after game startup. Changes to the file are not applied to an already running process; exit and restart the game after every edit. Restarting only the PowerShell client is not sufficient.

When `scenario_start` needs to enqueue a new request, the plugin resolves the configured queue symbol in the parser and checks that it is a scalar integer variable (`zPAR_TYPE_INT`, one element) before reading or writing it. On every `scenario_status` read, the plugin checks each configured status symbol in the same way. The queue `request_symbol` must also be writable, meaning it cannot be a constant. A wrong type or array returns `configured_symbol_type_mismatch`; a constant queue returns `configured_symbol_not_writable`. `ping` confirms that the configuration file and mappings are valid, but it does not confirm that the symbols exist or have the right type in the currently loaded parser.

## Agent usage pattern

1. Call ping and inspect protocol_version, world_ready, and capabilities.
2. Use vob, raycast, raycast_batch, or surface_grid for observation.
3. Before camera_set or scenario_start, verify its mutating capability is true and obtain explicit task-level authorization from the user or calling system.
4. Use a unique request ID, include an expiry, and wait for the response with that ID.
5. On client timeout, write cancel.<id> and wait for the cancellation response before sending another request.
6. After camera movement, call camera_restore and check its restored result.

For a current expiry value, calculate the Unix time in milliseconds at request creation and add the chosen timeout duration. Do not reuse the example timestamp from this document.
