[English](README.md) | [Polski](README.pl.md)

# ZenGin DevBridge

ZenGin DevBridge is a local file based diagnostic plugin for **Gothic II: Night of the Raven (x86)**. It lets a local script or AI agent inspect the loaded world through the Union API without adding a network listener.

Version `0.1.0` is a public preview. It is intended for modders who can build and test a Union plugin and understand that the game is a live process.

## Included tools

- Read a VOb's class, transform, bounds, and collision flags.
- Trace a ray or a bounded batch of rays.
- Sample a bounded X/Z `surface_grid`, including static-world, dynamic-VOb, bounding-box, and player floor/water/ceiling comparisons.
- Read camera transforms and AI flags.
- Optionally set and restore the camera. Camera changes are disabled by default.
- Optionally dispatch and read configured Daedalus integer symbols. Scenario dispatch is disabled by default and has no built-in scenario names.
- Read protocol version and command capabilities with `ping`.

Read-only commands are available by default. `camera_set` and `scenario_start` change game state and require local opt-in. `camera_restore` remains available to clear a DevBridge camera override.

## Requirements

- Windows 10 or later.
- Gothic II: Night of the Raven, x86, with a compatible Union installation.
- Visual Studio 2022 C++ x86 build tools and a Windows 10 SDK.
- CMake 3.24 or newer, Ninja, and Git.

The project builds against the pinned Union API and Gothic API submodules. It does not include private project content or game assets.

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
   .\client\powershell\Query-DevBridge.ps1 -GameRoot 'C:\Games\Gothic II'
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
  "allow_scenario_start": false
}
```

Set `allow_camera_control` to `true` only when you want to permit `camera_set`. Camera restore is always accepted. Scenario dispatch additionally needs a local `scenario_dispatch` mapping; see [the API and configuration reference](docs/API.md#local-configuration). Keep this file local and review every symbol mapping before enabling it.

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

See [docs/API.md](docs/API.md) for the complete request format, response fields, errors, and mutation safeguards.

### Codex and Claude Code

Codex, Claude Code, or another coding agent can call the included PowerShell client from its terminal tool and parse the returned JSON. For example, ask the agent to inspect the current player and nearby floor without changing game state:

```text
Use the ZenGin DevBridge PowerShell client. First call ping and check world_ready and capabilities. Then inspect PC_HERO with vob and sample the floor with surface_grid. Report the JSON findings. Do not call camera_set or scenario_start.
```

When you explicitly authorize a camera change, have the agent check that `camera_set` is enabled, perform the requested action, and call `camera_restore` afterward. Scenario dispatch requires its own explicit authorization and a reviewed local symbol mapping. The API guide documents the same workflow for direct JSON-file clients.

## Compatibility and support

- This preview targets Gothic II: Night of the Raven only.
- The isolated testbed was verified with Gothic II 2.6 (fix) and Union 1.0M.
- Requests without `protocol_version` are accepted as protocol version 1 to preserve existing clients.
- Environment-specific scenario names and Daedalus symbols belong in the local configuration, never in the public plugin source.
- For a verified build or later deployment, record the source revision, plugin version, and DLL SHA-256.

## Basic protocol checks

The included mock bridge checks that the PowerShell client sends protocol version 1 and a future expiry, handles a matching response, and sends a cancellation marker after its timeout:

```powershell
pwsh -NoProfile -File .\tests\Invoke-MockProtocolTests.ps1
```

This verifies the client file workflow. Runtime smoke checks and a short idle comparison were also run in the isolated testbed; see [docs/TESTBED-STATUS.md](docs/TESTBED-STATUS.md) for scope and limits. Results can differ with another Gothic installation, Union build, or mod.

## License

The DevBridge source is MIT licensed. Third-party code and required notices are listed in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). The project does not redistribute the Union plugin template used during early development; its repository does not publish a license, so its scaffold and other files are excluded.
