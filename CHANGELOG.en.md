[🇵🇱 Polski](CHANGELOG.md) | [🇬🇧 English](CHANGELOG.en.md)

# Changelog

## 0.2.0 — 2026-11-27 (pre-release)

- Expanded the API with bounded VOb search and hierarchy, NPC diagnostics, and detailed player state and collision reads.
- Added waypoint and local WayNet graph inspection, disconnected/dangling-link detection, and routes computed with bounded Dijkstra. A graph result explicitly does not confirm physical walkability.
- Added Daedalus symbol search and bounded typed reads; the plugin does not write arbitrary variables.
- Added zCMover state reads and bounded monitoring of the player, an NPC, or a mover, with time and sample limits, cancellation, and no-progress handling.
- Added screenshots of the current renderer frame as TGA in the controlled exchange directory.
- Added optional player_teleport, player_turn, player_move, and player_walk_to commands. They are disabled by default and restore the prior input state when an operation ends.
- Movement commands supply input before the player's AI is processed and report the number of AI frames (`control_ai_frames`), so clients can distinguish request handling from actual movement.
- Added a player-control regression script that finds a clear direction and checks actual movement and target arrival in a controllable world.
- Kept protocol version 1 and existing v0.1.1 commands; the PowerShell client accepts the new command names.
- The final source review found that a configured int class variable has no instance context for GetValue/SetValue. Validation in scenario_start and scenario_status now rejects such symbols with configured_symbol_type_mismatch; the queue must still be writable.
- Fixed horizontal heading normalization before SetHeadingAtWorld so a target vector does not distort the player's orientation.
- Final Windows x86 DLL: SHA-256 B28E10A4441DA8DE7687C60AF2E99B27B1B7F3B0A8F21E063A737032216D4CC4. This file passed 11/11 runtime checks and 7/7 backward-compatibility checks.
- On the final DLL, global float, string, and array symbols were read; a classvar was safely rejected as both queue and status; a valid global int worked as queue and status. player_turn changed the heading to [1,0,0] and restored the previous direction; player_teleport moved the hero 100 X units and returned to the starting position.
- mover_inspect returned state for an existing zCMover; the monitor sampled Xardas and a mover over four samples. screenshot saved a valid 800×600×32 TGA image, which was visually checked and removed from the testbed.
- vob_hierarchy returned a valid target but no ancestors or children in the tested world; a non-empty tree relationship was not confirmed.
- The movement hook on the final DLL received input over 74 AI frames in a dialogue scene, but distance_moved was only 0.023 units; input was released. A later test from a loaded outdoor save confirmed player_move forward (178.11 units / 713 ms), player_walk_to reaching two targets, and movement_stalled on a steep slope (188.71 units moved, 96.12 units from target, input_released=true). On open ground, plugin strafe tests over 87 and 83 AI frames did not change X/Z (distance_moved was 0.092 and 0.108 from Y change only); input was released. A manual hold of the ordinary Strafe Left and Strafe Right keys in the same profile and spot moved the hero about +351 and −659 X units respectively. Engine movement worked; the limitation is in the plugin input path, so player_move strafing remains experimental.
- A roughly 74.646-unit discrepancy between the end position in one movement response and a later player_state was checked in source: both commands read the same VOb world position and use the Euclidean norm. The extra vector is almost exactly parallel to the preceding movement and corresponds to about 0.30 seconds of walking at the measured speed. It is consistent with a later world-state read; the exact time gap and input state were not recorded, so a specific cause cannot be confirmed. No measurement bug was found.
- With the default configuration, ping reported player control disabled and player_move returned capability_disabled.
- A long-monitor cancellation test returned timeout, cleared the active request, and left ping operational. The only performance measurements with a complete local log used comparison DLL SHA-256 `89B34B07E6A8F3813B7284D71F8B90FCA300DCA18AB9D0EE230CF8A5311D43F5`, not final DLL B28. In a 15-second idle sample, Gothic used 15.406 s CPU / 15.025 s wall time (102.54% of one core) and memory changed from 392.5 to 393.5 MiB; without the plugin it used 15.031 s / 15.039 s (99.95%) and memory changed from 388.1 to 386.8 MiB. Across eight VOb scans, total elapsed time was 13.347 s, mean 1,662 ms per request, 97.51% of one core, and a mean of 16,816 nodes; request timing included launching a separate PowerShell client. These results do not isolate plugin overhead. No comparable performance measurement is available for DLL B28, so measurements from other runs are not attributed to the final file.
- The final DLL B28 confirmed forward movement, target arrival, and stall detection; ordinary keyboard keys allowed lateral movement, while plugin strafing did not change X/Z. Lateral control remains experimental; the cause of the additional displacement between position reads was not confirmed.
- Published [v0.2.0 as a pre-release](https://github.com/Scammers7531/ZenGin-DevBridge/releases/tag/v0.2.0). ZIP package (399,112 bytes), SHA-256 `4526FF587D4215DFF90CA50773C61BBF92B44D7F6341FF467DCA20E8B90BE05B`. Subsequent editorial corrections on main do not change the published package or tag.

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
