# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project uses independent Windows and Android version numbers that are
bundled under one GitHub release tag.

## [Unreleased]

## [0.10.2] - 2026-11-23

### Added

- Android 0.4.4: grouped SDK-native sliders and dropdowns, exact exposure units,
  linear/log scale, actual camera readback and acquisition locks. The signed APK
  keeps the existing distribution key.

### Fixed

- ZWO settings now have grouped sliders, exact numeric inputs and supported
  dropdowns, with exposure units, generated quick picks, optional manual Apply
  and per-model presentation preferences (visibility, order, step and log scale).

- ASI Live View now packs the BGR pixel layout accepted by the Windows JPEG encoder;
  Bayer red/blue channels remain correctly ordered.
- Preview decoding initializes COM independently of the Canon backend, including
  the ZWO camera worker and its image analysis.
- SDK-native control names no longer disappear beyond full-width inputs. The
  connected model's panel shows exact SDK ranges, persistent edits, Apply,
  authoritative readback and supported automatic modes. Native telemetry refreshes
  once per second; edits are locked during acquisition.
- The Canon-only Bulb switch is hidden for ASI cameras.

### Validation

- Physical ASI662MC: native Gain write/readback, 24 decoded Live View JPEGs,
  a RAW16 FITS exposure and return to video in the same session. Tests restore
  the camera's original gain/exposure. Other camera applications must disconnect
  before acquisition.

## [0.10.1] - 2026-11-23

### Added

- Android 0.4.3 companion with SDK-native numeric controls, range validation and automatic-mode switches.
- FITS files in the remote PC gallery.
- ZWO ASI camera module with SDK-driven controls, shared Live View and capture
  ownership, and RAW16 FITS acquisition.
- True North compass with ASTAP plate solving, ERFA 2.0.1 precession/nutation
  and explicit physical-contract confirmations.
- `--true-north-demo=1` command-line mode that opens a supplied image and starts
  a new ASTAP solve without reusing previous coordinates.
- Numerical five-frame RAW16 FITS stacking test, including valid gain zero.
- Source-layout and public-header boundary checks in CMake and PowerShell.

### Changed

- Sources, headers, resources and tests are organized by feature; all includes
  and CMake inputs were updated.
- All three supplied sky fields solve through fresh ASTAP centroid extraction
  and C++ Gaia polar matching, with unique correspondences, confidence rejection,
  measured field dimensions and the purple celestial-axis marker.

### Fixed

- RAW16 Bayer FITS preview no longer overflows the stack: LibRaw instances are
  heap-allocated in calibration, RAW decoding and CR2 preview.

## [0.9.2] - 2026-11-19

Windows 0.9.2 and Android 0.4.2.

### Added

- Crescent moon blueprint icon shared by the Windows executable, MSI shortcut,
  Android launcher and website favicon.
- Android adaptive launcher icon for Android 8.0 and newer, with density-matched
  48 to 192 pixel legacy mipmaps and 108 to 432 pixel foreground layers.

### Changed

- Windows version is 0.9.2; Android version is 0.4.2 (`versionCode` 8).
- `scripts/build-icon.py` now regenerates both the Windows ICO and the Android
  launcher assets from `assets/astrophoto.svg`.

### Removed

- The previous lens-and-star icon and the Android vector launcher drawable.

## [0.9.1] - 2026-11-19

Windows 0.9.1 and Android 0.4.1.

### Added

- Windows and Android check the published GitHub releases on startup and tell
  the user when a newer platform package exists.
- English update dialog with release notes, `Open release page`, `Later` and
  `Skip this version`; the skipped version survives restarts.
- Windows `Settings` menu shows the installed version and offers
  `Check for updates`, including explicit error reporting for manual checks.
- Windows update checks use WinHTTP with request timeouts and run off the UI
  thread; Android checks use OkHttp and are cached for 24 hours.
- Unit tests for version comparison, release filtering, platform asset
  selection, malformed payloads and untrusted release URLs on both platforms.

### Changed

- The GitHub repository for update checks is configured once per platform:
  `ASTRO_UPDATE_REPOSITORY` in CMake and `astrophotoUpdateRepository` in Gradle.
- Versions, release notes and download links are read from the GitHub API at
  runtime; no release data is compiled into either client.
- Windows version is 0.9.1; Android version is 0.4.1 (`versionCode` 7).

### Fixed

- The remote security note in the Windows settings and on the website now
  correctly states that traffic is authenticated and encrypted instead of
  claiming it is unencrypted.

## [0.9.0] - 2026-11-15

Windows 0.9.0 and Android 0.4.0.

### Added

- Encrypted remote protocol v2: every PC and phone payload, including Live View
  JPEGs, is sealed with AES-256-GCM using a per-connection key.
- Fast RGB Drizzle 1.5x, 2x and 3x with an automatic memory budget.
- `Open session folder` action in Files and session.
- Point autofocus retargeting while a run is in progress.

### Changed

- Shots are released without the camera's own autofocus to avoid focus drift
  and ghosting in unaligned stacks.
- Inter-frame delay overlaps processing, star detection is skipped when
  alignment is off, and sigma rejection is more robust with few frames.
- Windows and Android peers using protocol v1 are rejected.

## [0.8.0] - 2026-11-02

Windows 0.8.0 and Android 0.3.0.

### Added

- Sensor Bias, Dark, Flat and Dark-flat calibration with dedicated acquisition.
- Safe Pause and Resume, explicit or optional startup CR2 rebuild, homogeneous
  sequence plans, background normalization and linear-gradient correction.
- Bounded quad/affine registration recovery, online Winsor limits and float32
  TIFF export.
- Stellar HFD/FWHM diagnostics and optional external ASTAP astrometry.
- Night (red) theme on Windows.

### Fixed

- MCP port conflicts no longer prevent the desktop app from starting.
