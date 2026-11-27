[🇵🇱 Polski](README.md) | [🇬🇧 English](README.en.md)

# ZenGin DevBridge

ZenGin DevBridge is a local file based diagnostic plugin for **Gothic II: Night of the Raven (x86)**. It lets a local script or AI agent inspect the loaded world through the Union API without adding a network listener.

Version `0.1.1` is a pre-release verified in an isolated testbed running Gothic II: Night of the Raven 2.6 (fix) with Union 1.0M. You can use the ready-to-use DLL or build the project from source; the requirements for these two paths are listed separately below. The plugin runs inside the active game process.

**Ready-to-use pre-release:** [Download ZenGinDevBridge-0.1.1-win32.zip](https://github.com/Scammers7531/ZenGin-DevBridge/releases/download/v0.1.1/ZenGinDevBridge-0.1.1-win32.zip) · [release notes and SHA-256 checksums](https://github.com/Scammers7531/ZenGin-DevBridge/releases/tag/v0.1.1). Development tools are not required to use the DLL.

## Included tools

- Read a VOb's class, transform, bounds, and collision flags.
- Trace a ray or a bounded batch of rays.
- Sample a bounded X/Z `surface_grid`, including static-world, dynamic-VOb, bounding-box, and player floor/water/ceiling comparisons.
- Read camera transforms and AI flags.
- Optionally set and restore the camera. Camera changes are disabled by default.
- Optionally dispatch and read configured Daedalus integer symbols. Scenario dispatch is disabled by default and has no built-in scenario names.
- Read protocol version and command capabilities with `ping`.

Read-only commands are available by default. camera_set, scenario_start, player_teleport, player_turn, player_move, and player_walk_to change game state and require local opt-in plus a capabilities check with ping. camera_restore remains available to clear a DevBridge camera override.

## Practical uses

- Check whether `PC_HERO` or another VOb exists in the loaded world and inspect its class, transform, and bounds.
- Compare visual geometry, the static world, and dynamic VObs to find collision discrepancies with `raycast`, `raycast_batch`, and `surface_grid`.
- Save results as JSON or CSV and compare them across map or mod configuration changes.
- Set the camera for a repeatable scene review when camera control is enabled locally, then restore the original setup with `camera_restore`.
- In a test installation, dispatch a locally configured scenario and read configured counters/status values without adding scenario names to the public plugin.

## Local v0.2.0 candidate

The latest public release is still v0.1.1. The additions below are a locally prepared v0.2.0 candidate; they are not published and do not have a public download package yet.

The candidate adds:

- bounded VOb search, hierarchy inspection, and NPC search and inspection;
- detailed player state and collision, support, falling, and obstacle reads;
- waypoint inspection, WayNet graph and connectivity analysis, and routes computed with Dijkstra's algorithm;
- Daedalus metadata search and typed reads of int, float, and string values, including bounded array ranges;
- zCMover state and keyframe inspection; the command does not activate movers;
- time-limited monitoring of the player, an NPC, or a mover, with a sample cap and no-progress detection;
- screenshots of the current renderer frame saved as TGA in the exchange directory;
- optional, disabled-by-default player_teleport, player_turn, player_move, and player_walk_to commands.

Player control requires explicit allow_player_control:true in local configuration. Teleporting is not movement. player_walk_to sets a heading and emulates forward input; it does not compute a path or avoid obstacles. An earlier candidate build traversed a route in a controllable world, but the final DLL with the heading-normalization fix was not retested in free gameplay. On the final DLL in a dialogue scene, input reached the hook for 74 AI frames but distance moved was only 0.023 units. Treat actual walking as experimental. Neither control_ai_frames nor reason:duration_elapsed proves movement; check distance_moved.

The final local candidate DLL has SHA-256 B28E10A4441DA8DE7687C60AF2E99B27B1B7F3B0A8F21E063A737032216D4CC4. On that file, 11/11 runtime checks and 7/7 backward-compatibility checks passed; additional checks confirmed real heading and position changes, Daedalus symbols, NPC/mover monitoring, and a valid TGA image. In the tested world, vob_hierarchy returned a valid target but empty ancestor and child lists. A real route-traversal test on this exact DLL remains incomplete, so the candidate is not yet ready for public release. Detailed local testbed data and game files remain outside the public repository.

### Examples for Codex, Claude Code, and custom scripts

Codex, Claude Code, and a custom script can call the same PowerShell client and inspect its JSON. The following queries are read-only:

    $client = '.\client\powershell\Query-DevBridge.ps1'
    & $client -GameRoot 'C:\Games\Gothic II' -Command vob_search -PayloadJson '{"class_contains":"zCMover","limit":20,"scan_limit":20000}'
    & $client -GameRoot 'C:\Games\Gothic II' -Command waypoint_search -PayloadJson '{"limit":64,"offset":0}'
    & $client -GameRoot 'C:\Games\Gothic II' -Command daedalus_read -PayloadJson '{"symbol":"TRUE","count":1}'

In a Codex or Claude Code instruction, ask the agent to check capabilities with ping, inspect the returned JSON, and distinguish unavailable data from zero values. It should state that a WayNet route and raycasts do not confirm that the character can traverse the world. State-changing commands require separate explicit authorization and local configuration opt-in.

### Exporting results to JSON and CSV

The included client saves the complete JSON response with -SaveJsonPath. -ExportCsvPath writes flattened CSV rows for raycast_batch and surface_grid:

    $client = '.\client\powershell\Query-DevBridge.ps1'
    $gameRoot = 'C:\Games\Gothic II'
    $payload = @{ default_flags = 1058; probes = @(@{ start = @(100, 1000, 200); direction = @(0, -1500, 0) }) } | ConvertTo-Json -Depth 4 -Compress
    & $client -GameRoot $gameRoot -Command raycast_batch -PayloadJson $payload -SaveJsonPath '.\raycasts.json' -ExportCsvPath '.\raycasts.csv'

## Requirements

### Using the ready-to-use DLL

- Windows 10 or later.
- Gothic II: Night of the Raven, x86, with a compatible Union installation.
- Windows PowerShell 5.1 or PowerShell 7 when using the included client; the test scripts require PowerShell 7 (pwsh).
- Download the [ready-to-use ZIP](https://github.com/Scammers7531/ZenGin-DevBridge/releases/download/v0.1.1/ZenGinDevBridge-0.1.1-win32.zip) or review the [release notes](https://github.com/Scammers7531/ZenGin-DevBridge/releases/tag/v0.1.1).

Visual Studio, CMake, Ninja, and Git are not required to run the ready-to-use DLL. Gothic II and Union must already be installed.

### Building from source

- Windows with Visual Studio 2022 C++ Build Tools for x86 and the Windows 10 SDK.
- CMake 3.24 or newer, Ninja, and Git.

The game and a compatible Union installation are needed for runtime use and testing, but not for compilation itself. The project builds against the pinned Union API and Gothic API submodules; the repository does not include private project content or game assets.

## Build

Clone with all source dependencies:

```powershell
git clone --recurse-submodules https://github.com/Scammers7531/ZenGin-DevBridge.git
cd ZenGin-DevBridge
```

The build script takes paths to the local compiler toolchain, CMake, Ninja, and Windows SDK. Typical Visual Studio installations place the SDK separately from the compiler:

```powershell
.\scripts\build.ps1 `
  -ToolchainRoot 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools' `
  -WindowsSdkRoot 'C:\Program Files (x86)\Windows Kits\10' `
  -CMakePath 'C:\Program Files\CMake\bin\cmake.exe' `
  -NinjaPath 'C:\Program Files\CMake\bin\ninja.exe'
```

The resulting DLL is `build\g2a-release\ZenGinDevBridge.dll`. The script does not install or deploy the plugin.

## Install

1. Back up the target game's existing Union configuration and close the game.
2. Copy `ZenGinDevBridge.dll` into `<game-root>\System\Autorun\`.
3. Start Gothic II: Night of the Raven. The plugin creates `<game-root>\ZenGinDevBridge\` when its render hook runs.
4. Run the PowerShell client from this repository, supplying the game path:

   ```powershell
   .\client\powershell\Query-DevBridge.ps1 -GameRoot 'C:\Games\Gothic II' -Command ping
   ```

   Alternatively, use `-ExchangeDirectory` to point the client at an existing exchange folder.

The plugin and client exchange UTF-8 JSON files locally. The plugin does not start a server or send telemetry. Keep the exchange folder writable by the game process.

## Local configuration

Place `ZenGinDevBridge.config.json` in the game root, next to the `System` folder. No configuration is required for read-only tools.

```json
{
  "config_version": 1,
  "exchange_directory": "ZenGinDevBridge",
  "allow_camera_control": false,
  "allow_scenario_start": false,
  "allow_player_control": false
}
```

Set `allow_camera_control` to `true` only when you want to permit `camera_set`. Camera restore is always accepted. Scenario dispatch additionally needs a local `scenario_dispatch` mapping; see [the API and configuration reference](docs/API.en.md#local-configuration). Keep this file local and review every symbol mapping before enabling it.

The plugin loads configuration when its render hook first runs after game startup and caches it for the lifetime of the process. Exit and restart the game after every configuration change. Restarting only the PowerShell client does not reload the plugin configuration.

## AI agent examples

Use the included client for a single read-only query:

```powershell
$client = '.\client\powershell\Query-DevBridge.ps1'
& $client -GameRoot 'C:\Games\Gothic II' -Command ping
& $client -GameRoot 'C:\Games\Gothic II' -Command vob -Name PC_HERO
& $client -GameRoot 'C:\Games\Gothic II' -Command raycast `
  -Start 100,1000,200 -Direction 0,-1500,0 -Flags 1058
```

An agent that speaks the file protocol directly should send one request at a time, include protocol version 1 and an expiry time, wait for the matching `response.json` ID, and treat `ok: false` as an error. The JSON below shows the request shape; calculate a fresh `expires_at_unix_ms` value for each request rather than copying the illustrative timestamp:

```json
{
  "id": "agent-0001",
  "command": "surface_grid",
  "protocol_version": 1,
  "expires_at_unix_ms": 1795700000000,
  "center": [100, 1000, 200],
  "radius_x": 20,
  "radius_z": 20,
  "spacing": 20,
  "down_range": 2000,
  "include_bbox": false
}
```

See [docs/API.en.md](docs/API.en.md) for the complete request format, response fields, errors, and mutation safeguards.

### Codex and Claude Code

Codex, Claude Code, or another coding agent can call the included PowerShell client from its terminal tool and parse the returned JSON. For example, ask the agent to inspect the current player and nearby floor without changing game state:

```text
Use the ZenGin DevBridge PowerShell client. First call ping and check world_ready and capabilities. Then inspect PC_HERO with vob and sample the floor with surface_grid. Report the JSON findings. Do not call camera_set, scenario_start, or player_*.
```

When you explicitly authorize a camera change, have the agent check that `camera_set` is enabled, perform the requested action, and call `camera_restore` afterward. Scenario dispatch requires its own explicit authorization and a reviewed local symbol mapping. The API guide documents the same workflow for direct JSON-file clients.

## Compatibility and support

- This preview targets Gothic II: Night of the Raven only.
- The isolated testbed was verified with Gothic II 2.6 (fix) and Union 1.0M.
- Requests without `protocol_version` are accepted as protocol version 1 to preserve existing clients.
- Environment-specific scenario names and Daedalus symbols belong in the local configuration, never in the public plugin source.
- For a verified build or later deployment, record the source revision, plugin version, and DLL SHA-256.

## Basic protocol checks

The included checks cover the protocol exchange, timeout cancellation, and two clients running concurrently against one exchange folder. The lock regression test confirms that a client that did not acquire the lock cannot remove the lock or request owned by the first client:

```powershell
pwsh -NoProfile -File .\tests\Invoke-MockProtocolTests.ps1
pwsh -NoProfile -File .\tests\Invoke-ClientLockTests.ps1
```

This verifies the client file workflow. Run player-control checks only in an isolated testbed with allow_player_control:true, after scripted scenes have ended and with nearby ground clear of obstacles. Replace `<testbed-path>` with the testbed's actual game-root path. The script selects a direction with a clear short raycast, checks actual movement, and verifies walk-to completion:

```powershell
pwsh -NoProfile -File .\tests\Invoke-PlayerControlTests.ps1 -GameRoot '<testbed-path>'
```

Results can differ with another Gothic installation, Union build, or mod.

## License

The DevBridge source is MIT licensed. Third-party code and required notices are listed in [THIRD_PARTY_NOTICES.en.md](THIRD_PARTY_NOTICES.en.md). The project does not redistribute the Union plugin template used during early development; its repository does not publish a license, so its scaffold and other files are excluded.
