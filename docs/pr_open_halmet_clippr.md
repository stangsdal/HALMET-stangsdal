# HALMET PR Title

Refactor standard mode and externalize Clipper as standalone module

# HALMET PR Body

## Summary

This PR restructures HALMET firmware by extracting non-Clipper setup into a dedicated standard-mode module and moving Clipper functionality out of the HALMET source tree into an external dependency.

## Key Changes

- Standard mode extraction
  - Add src/standard_mode.cpp
  - Add src/standard_mode.h
  - Add src/standard_mode_defaults.h
  - Update src/main.cpp to call SetupStandardMode(...)

- Clipper externalization
  - Remove in-tree files:
    - src/clipper_feature.cpp
    - src/clipper_feature.h
  - Add external git dependency in platformio.ini:
    - sensesp_clipper_input=https://github.com/stangsdal/sensesp_clipper_input.git

- Mapping and sender updates
  - Add PGN 130312 sender support in src/n2k_senders.h
  - Add docs/mapping.md with input to Signal K / NMEA 2000 mapping

- Documentation
  - Update README.md to reflect actual mappings and external Clipper module location
  - Add docs/pr_summary_clipper_external_module.md

## Why

- Improves maintainability and separation of concerns
- Enables independent release cadence for Clipper integration
- Makes Clipper reusable in other SensESP projects

## Validation

- Editor diagnostics: no errors on touched files
- Build validation should be run for:
  - stangsdal
  - stangsdal_clipper

## Migration Notes

- First build after clean checkout requires dependency fetch from GitHub
- If dependency resolution fails, clear PlatformIO cache and rebuild

## References

- External module repo:
  - https://github.com/stangsdal/sensesp_clipper_input
- Mapping doc:
  - docs/mapping.md

## Suggested PR Settings

- Base branch: main
- Compare branch: clipper
