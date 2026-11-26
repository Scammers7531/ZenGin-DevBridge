[🇵🇱 Polski](CHANGELOG.md) | [🇬🇧 English](CHANGELOG.en.md)

# Changelog

## 0.1.1 — 2026-11-26 (package verified, unpublished)

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
