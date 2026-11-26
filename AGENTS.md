# Development guidance

## Canonical source and deployment

- This repository is the canonical source project for ZenGin DevBridge.
- Maintain one DevBridge implementation. Keep environment-specific behavior in local configuration rather than in a consumer-specific fork.
- Develop universal features and fixes here first. Verify them in an isolated runtime before planning a separate transfer to a consuming installation.
- Never automatically copy, overwrite, or deploy builds to a consuming or production installation. A deployment must be a separate deliberate step with a backup and a record of source revision, plugin version, and deployed DLL SHA-256.

## Compatibility and project boundaries

- Preserve compatibility with existing test clients: keep established commands, request payloads, response fields, and the one-request-at-a-time file workflow when possible.
- Keep protocol additions backward-compatible. Requests without `protocol_version` are treated as protocol version 1; version additions and any breaking changes must be documented in both `docs/API.md` and `docs/API.en.md`, and both `CHANGELOG.md` and `CHANGELOG.en.md`.
- Use local configuration for environment-specific exchange-directory names, scenario names, and Daedalus symbol mappings. Do not hard-code private scenario names or symbols into the public core.
- Do not include private scripts, story content, game assets, logs, local audit reports, personal data, toolchains, compiler files, or Windows SDK files in public commits or release packages.
- Do not add MCP, NPC diagnostics, or support for other Gothic versions to this preview.

## Engineering and release

- Keep changes narrow and preserve working universal diagnostics, raycasts, `surface_grid`, and camera tools.
- Treat camera changes and scenario dispatch as state-changing operations. Keep them disabled by default, expose their availability through capabilities, and retain a safe restore path.
- Keep the file protocol local. Validate request size, IDs, numeric inputs, deadlines, and world identity before using game pointers.
- Keep third-party source pinned to reviewed commits and preserve each dependency's required notices in `THIRD_PARTY_NOTICES.md` and binary release materials.
- Record supported game/Union versions, protocol changes, release version, and the built DLL SHA-256 in project documentation.
- Build and test in an isolated game copy when runtime validation is needed. Do not alter original consumer or production installations while developing this canonical project.
- Keep work local until the maintainer explicitly approves publishing. Never push this repository to GitHub without that approval.
- Polish is the default public documentation language: use `README.md`, `docs/API.md`, `CHANGELOG.md`, and `docs/TESTBED-STATUS.md` for Polish, with matching English files `README.en.md`, `docs/API.en.md`, `CHANGELOG.en.md`, and `docs/TESTBED-STATUS.en.md`. Update each language pair in the same change. Keep the language switches visible and preserve command names, protocol fields, and code examples exactly.
