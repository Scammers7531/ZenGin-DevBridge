[🇵🇱 Polski](README.md) | [🇬🇧 English](README.en.md)

# ZenGin DevBridge

A local diagnostics and automation plugin for **Gothic II: Night of the Raven (x86)**. It exposes live ZenGin engine data through Union API so modders, scripts and AI agents can inspect the game world and run controlled gameplay tests. Communication uses local JSON files; the plugin opens no network listener and sends no telemetry.

**Current release: v0.2.0 (Pre-release) · 31 API commands · protocol 1 · MIT license**

[Download ZIP](https://github.com/Scammers7531/ZenGin-DevBridge/releases/download/v0.2.0/ZenGinDevBridge-0.2.0-win32.zip) · [Release notes](https://github.com/Scammers7531/ZenGin-DevBridge/releases/tag/v0.2.0) · [ZIP SHA-256](https://github.com/Scammers7531/ZenGin-DevBridge/releases/download/v0.2.0/SHA256SUMS-v0.2.0.txt)

## Capabilities

| Area | What it provides |
| --- | --- |
| World objects | VOb inspection, search and available hierarchy relationships. |
| Geometry and collision | Single and batched raycasts, surface grids, player ground and obstacle probes. |
| NPCs and player | Available position, health, AI, routine, animation, inventory and movement data. |
| WayNet | Waypoint search, graph analysis and graph routes. |
| Daedalus | Symbol search, typed int, float, string and bounded array reads; optional dispatch of configured scenarios. |
| Moving objects | zCMover state, parameters and keyframes. |
| Camera and images | Camera inspection, optional set and restore, and TGA frame capture. |
| Player control | Optional teleport, turn, actual movement attempts and walk-to attempts. |
| Monitoring | Time-bounded sampling of player, NPC or mover state. |

Example uses include diagnosing stair and platform collisions, inspecting NPC and quest state, checking WayNet connectivity, comparing object positions and running repeatable movement attempts in a test installation.

## Requirements

- Windows 10 or later; Gothic II: Night of the Raven x86 with compatible Union.
- Verified with Gothic II 2.6 (fix) and Union 1.0M. Other configurations need separate testing.
- Windows PowerShell 5.1 or PowerShell 7 for the client; test scripts require PowerShell 7.
- The ready-to-use DLL does not require a compiler, CMake, Ninja or Git.

## Quick start

1. Download and extract [v0.2.0](https://github.com/Scammers7531/ZenGin-DevBridge/releases/download/v0.2.0/ZenGinDevBridge-0.2.0-win32.zip).
2. Close the game and back up the Union configuration and any existing DLL.
3. Copy `ZenGinDevBridge.dll` to `<game-root>\System\Autorun\`.
4. Start the game and load a world. Wait for the exchange folder `<game-root>\ZenGinDevBridge\` to appear.
5. Open PowerShell in the extracted package directory and adjust the game path:

```powershell
$client = '.\client\powershell\Query-DevBridge.ps1'
$gameRoot = 'C:\Games\Gothic II'
& $client -GameRoot $gameRoot -Command ping
& $client -GameRoot $gameRoot -Command player_state
```

Check `ok`, `plugin_version`, `protocol_version`, `world_ready` and `capabilities` in the ping response before further queries. The exchange folder must be writable; the client also accepts `-ExchangeDirectory`.

## Usage examples

```powershell
& $client -GameRoot $gameRoot -Command vob -Name PC_HERO
& $client -GameRoot $gameRoot -Command vob_search -PayloadJson '{"class_contains":"zCMover","limit":20}'
& $client -GameRoot $gameRoot -Command waypoint_search -PayloadJson '{"limit":20}'
& $client -GameRoot $gameRoot -Command daedalus_read -PayloadJson '{"symbol":"TRUE","count":1}'
& $client -GameRoot $gameRoot -Command screenshot
```

Save the complete JSON response using `-SaveJsonPath`; CSV export is intended for `raycast_batch` and `surface_grid`.

```powershell
$payload = @{ default_flags = 1058; probes = @(@{ start = @(100,1000,200); direction = @(0,-1500,0) }) } | ConvertTo-Json -Depth 4 -Compress
& $client -GameRoot $gameRoot -Command raycast_batch -PayloadJson $payload -SaveJsonPath '.\raycasts.json' -ExportCsvPath '.\raycasts.csv'
```

## AI agents

Codex, Claude Code and other tools with local terminal access can call the PowerShell client and analyze its responses. No MCP server is required. Example agent instruction:

> Check the version and capabilities with ping. Read player_state and player_collision and find nearby objects. Interpret the data, distinguish unavailable data from zero values, and do not change game state.

A client or agent sending JSON directly should serialize requests, use unique IDs and fresh deadlines, and wait for the matching response. See the [API](docs/API.en.md).

## Configuration and safety

Place the optional `ZenGinDevBridge.config.json` in the game root next to the `System` folder.

```json
{
  "config_version": 1,
  "exchange_directory": "ZenGinDevBridge",
  "allow_camera_control": false,
  "allow_scenario_start": false,
  "allow_player_control": false
}
```

Camera setting, scenario dispatch and player control are disabled by default. `camera_restore` remains available as a recovery operation; `player_state` and `player_collision` are diagnostic commands. `screenshot` writes an image file locally.

Restart the game after configuration changes. Use state-changing operations only with user authorization in a suitable test installation. Example after enabling `allow_player_control`, checking capabilities and completing dialogue scenes:

```powershell
& $client -GameRoot $gameRoot -Command player_move -PayloadJson '{"direction":"forward","duration_ms":1000}'
```

Check actual distance and `input_released`; a completed request does not prove movement. Scenario dispatch requires a separate symbol mapping described in the [API](docs/API.en.md#local-configuration).

## Known limitations

- `player_walk_to` does not avoid obstacles; WayNet routes and collision probes do not guarantee physical traversal.
- Forward walking and target arrival were confirmed. Lateral control remains experimental: in the tested configuration plugin input did not change X/Z, although ordinary keyboard input worked.
- `dialogue.active` can be false during a visible dialogue. Menu control, save/load operations and dialogue choice selection are not included in this release.
- Some scans have a bounded time budget and may return incomplete results. Mover inspection is passive; non-empty VOb hierarchy relationships were not confirmed in the tested scene.
- The final DLL passed 11/11 runtime checks and 7/7 compatibility checks in an isolated environment. No comparable performance benchmark was obtained for the final DLL, and not all Union configurations were tested.

## Build from source

Requires Visual Studio 2022 C++ Build Tools x86, Windows 10 SDK, CMake 3.24+, Ninja and Git. Adjust tool paths:

```powershell
git clone --recurse-submodules https://github.com/Scammers7531/ZenGin-DevBridge.git
cd ZenGin-DevBridge
git checkout v0.2.0
git submodule update --init --recursive
.\scripts\build.ps1 `
  -ToolchainRoot 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools' `
  -WindowsSdkRoot 'C:\Program Files (x86)\Windows Kits\10' `
  -CMakePath 'C:\Program Files\CMake\bin\cmake.exe' `
  -NinjaPath 'C:\Program Files\CMake\bin\ninja.exe'
```

Output: `build\g2a-release\ZenGinDevBridge.dll`. The script does not install the DLL. Protocol checks:

```powershell
pwsh -NoProfile -File .\tests\Invoke-MockProtocolTests.ps1
pwsh -NoProfile -File .\tests\Invoke-ClientLockTests.ps1
```

Run runtime and player-control tests only in an isolated game installation, after scenes that block movement have ended. See the [test report](docs/TESTBED-STATUS.en.md).

## Documentation and license

- [Full API and configuration](docs/API.en.md)
- [Changelog](CHANGELOG.en.md)
- [Test environment and results](docs/TESTBED-STATUS.en.md)
- [MIT license](LICENSE) and [third-party notices](THIRD_PARTY_NOTICES.en.md)

The v0.2.0 source tag still points to historical commit `117160a7042729dda08cd4f48e87b41f6f901267` and is not rewritten. Documentation on `main` may be newer than the tagged snapshot; download the current package from the [release page](https://github.com/Scammers7531/ZenGin-DevBridge/releases/tag/v0.2.0) and verify its ZIP using the [SHA-256 file](https://github.com/Scammers7531/ZenGin-DevBridge/releases/download/v0.2.0/SHA256SUMS-v0.2.0.txt).
