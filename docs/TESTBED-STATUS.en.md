[🇵🇱 Polski](TESTBED-STATUS.md) | [🇬🇧 English](TESTBED-STATUS.en.md)

# Isolated Runtime Testbed

Verification date: 2026-11-26

## Local boundary

The Gothic II runtime copy is under `build/testbed` and is excluded by the repository-root `/build/` ignore rule. Game files remain local and are not part of the public source or release package.

## Validated versions

- Gothic II: 2.6 (fix)
- Union: 1.0M
- ZenGin DevBridge: 0.1.0
- DevBridge protocol: 1

Built plugin SHA-256:

`DF60FBF41E927CFFC3F791B50979D64C5743DFD3E9D789FE16BD54EDE3CB9E3A`

## Runtime checks

The baseline launch without DevBridge and the launch with DevBridge both reached the Xardas intro scene with music and sound enabled. With DevBridge loaded from `System/Autorun`, the plugin reported `world_ready: true`.

The PowerShell client's `ping` returned `ok: true`, protocol version 1, and plugin version 0.1.0. Runtime checks passed for `ping`, `vob` on `PC_HERO`, `raycast`, a two-probe `raycast_batch`, and a full 121-sample `surface_grid`. The checked surface samples returned successfully and reported world-floor material data.

With default local settings, `camera_set` and `scenario_start` returned `capability_disabled`; `camera_get` succeeded. `camera_restore` with no active override returned safely without a crash. In a separate testbed-only run with camera control explicitly enabled, `camera_set` applied an override and `camera_restore` restored the saved transform and camera AI flags in the same world. The testbed configuration was reset to both state-changing capabilities disabled afterward.

The mock protocol test passed for protocol-version/deadline handling, matching response IDs, timeout cancellation, and cancellation acknowledgment. No crash was observed during these launch and command smoke checks. World replacement during an active operation was reviewed in the source but was not forced in a runtime test; this is not a long-run stability test.

## Short idle comparison

Two 15.02-second samples were taken in the same intro scene, once without the DevBridge DLL and once with it loaded. Process CPU time was 15.234 seconds without the plugin (101.43% of one logical CPU) and 15.188 seconds with it (101.08%). The observed difference was 0.35 percentage points of one logical CPU, within the noise of a short sample. No idle CPU overhead was measurable in this check. This does not measure active scan cost, other scenes, or long-term behavior.

The local runtime copy measured 2,840,313,904 bytes (2.840314 GB / 2.645248 GiB) at the latest local size check. This size is local-only and can change when the game installation changes.

No consumer or production installation was changed, and no build was deployed during verification.
