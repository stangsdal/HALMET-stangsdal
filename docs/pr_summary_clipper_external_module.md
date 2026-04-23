# PR Summary: Externalize Clipper Module and Refactor Standard Mode

## What this PR does

This PR completes a structural cleanup of HALMET firmware and moves Clipper functionality to a standalone module repository.

### 1. Externalizes Clipper feature implementation

- Removes in-tree Clipper implementation files from HALMET source tree.
- Uses external dependency instead:
  - Git dependency: https://github.com/stangsdal/sensesp_clipper_input.git
- Keeps application-level wiring in place via existing include and setup flow in main firmware.

### 2. Refactors default mode into a dedicated module

- Introduces standard mode encapsulation files:
  - src/standard_mode.cpp
  - src/standard_mode.h
  - src/standard_mode_defaults.h
- Reduces size and complexity of main startup path.
- Centralizes calibration and mapping defaults for maintainability.

### 3. Updates docs and mapping

- Updates README to reflect actual current signal and PGN mappings.
- Adds docs/mapping.md as implementation-aligned mapping reference.
- Updates documentation references for Clipper settings now that code lives in external repository.

## Why this change

- Improves maintainability by separating Clipper-specific code from HALMET core firmware.
- Makes Clipper functionality reusable in other SensESP projects.
- Reduces long-term risk of drift between docs and implementation.
- Enables independent iteration and release lifecycle for Clipper module.

## Breaking or behavior-impact notes

- Clipper source files are no longer present in HALMET src.
- Builds now fetch Clipper functionality from GitHub dependency.
- Runtime behavior and published Signal K / NMEA 2000 paths remain aligned with existing mapping intent.

## Testing status

- Workspace diagnostics for touched files: clean.
- Local agent shell cannot run platformio binary directly.
- Existing VS Code task builds in user environment previously succeeded.

## Migration notes for maintainers

1. Keep internet access available on first dependency resolve after clean build.
2. If dependency cache issues occur, clear PlatformIO package cache and rebuild.
3. Validate both environments after update:
   - stangsdal
   - stangsdal_clipper

## Follow-up suggestions

- Tag first HALMET release that depends on external Clipper module.
- Add pinned tag or commit for Clipper dependency once initial API stabilizes.
- Add CI matrix build for both stangsdal and stangsdal_clipper.
