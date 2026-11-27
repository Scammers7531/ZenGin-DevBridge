[🇵🇱 Polski](CHANGELOG.md) | [🇬🇧 English](CHANGELOG.en.md)

# Changelog

## 0.2.0 — local candidate, 2026-11-27 (unpublished)

- Expanded the API with bounded VOb search and hierarchy, NPC diagnostics, and detailed player state and collision reads.
- Added waypoint and local WayNet graph inspection, disconnected/dangling-link detection, and routes computed with bounded Dijkstra. A graph result explicitly does not confirm physical walkability.
- Added Daedalus symbol search and bounded typed reads; the plugin does not write arbitrary variables.
- Added zCMover state reads and bounded monitoring of the player, an NPC, or a mover, with time and sample limits, cancellation, and no-progress handling.
- Added screenshots of the current renderer frame as TGA in the controlled exchange directory.
- Added optional player_teleport, player_turn, player_move, and player_walk_to commands. They are disabled by default and restore the prior input state when an operation ends.
- Movement commands supply input before the player's AI is processed and report the number of AI frames (`control_ai_frames`), so clients can distinguish request handling from actual movement.
- Added a player-control regression script that finds a clear direction and checks actual movement and target arrival in a controllable world.
- Kept protocol version 1 and existing v0.1.1 commands; the PowerShell client accepts the new command names.
- Runtime checks in a running Gothic II 2.6 (fix) game with Union 1.0M covered communication, compatibility with established commands, new diagnostics, valid and invalid Daedalus symbol types, monitor cancellation, screenshots, and actual player_move and player_walk_to. Movement changed the player position, reached the target within tolerance, and released input.
- A long-monitor cancellation test returned timeout, cleared the active request, and left ping operational. The TGA screenshot was checked and removed from the exchange directory.
- Candidate Windows x86 DLL: SHA-256 89B34B07E6A8F3813B7284D71F8B90FCA300DCA18AB9D0EE230CF8A5311D43F5.
- This remains a local candidate; no tag, publication, or deployment to GO3 or GO3 DEV was made.

## 0.1.1 — 2026-11-26 (verified pre-release package)

- Split ready-to-use DLL requirements from source-build requirements; added a direct release link and practical plugin use cases.
- Expanded protocol documentation to cover every error code, rejections without `response.json`, deadlines, cancellation, camera restore, scenario retries, and configuration loading.
- Fixed `client.lock` cleanup so a client that did not acquire the lock cannot remove another process's lock file.
- Added checks that configured Daedalus symbols are one-element integer variables and that the queue symbol is not constant.
- Added a concurrent-client regression test; the `v0.1.0` tag and release remain unchanged.
- Verified the DLL in a running Gothic II 2.6 (fix) game with Union 1.0M: `ping`, `vob`, `raycast`, `surface_grid`, `camera_get`, and safe `camera_restore`; the protocol remains at version 1.
- Daedalus parser checks confirmed reading a scalar `int`, successful write/readback of a test queue, and rejection of a constant and an instance symbol with the expected error codes.
- SHA-256 of the local Windows x86 candidate build: `C4E8213AE394352550FF31709A83E6FEA8FC6118CCDA7435554BE47AB552644A`.

## Documentation update — 2026-11-26

- Set Polish as the default public documentation language and added full English counterparts with language switches.
- Added matching Polish and English API and isolated-testbed documentation.

## 0.1.0 Public Preview — 2026-11-26

- First standalone public preview for Gothic II: Night of the Raven (x86).
- Retained VOb inspection, raycast, batched raycast, surface grid, and camera inspection/control.
- Added protocol version 1, capability reporting, structured error codes, request deadlines, cancellation markers, and diagnostic logs.
- Made camera and scenario changes opt-in through a local game-root configuration file.
- Added world-change checks and safe camera restore behavior for unavailable or replaced worlds/cameras.
- Removed built-in environment-specific scenario names and symbol references.
- Added independent CMake build, installation and API documentation, and third-party notices.

### Known preview limits

- Only Gothic II: Night of the Raven is built and supported.
- Basic runtime behavior was verified on Gothic II 2.6 (fix) with Union 1.0M in an isolated testbed; other game and Union setups still need their own checks.
- A short idle comparison found no measurable overhead in the tested intro scene; it is not a long-run or cross-system performance benchmark.
- Scenario dispatch needs a locally configured Daedalus integer symbol and remains disabled by default.
- Camera control needs explicit local opt-in and can only restore saved transforms while the original world and camera objects remain available.
