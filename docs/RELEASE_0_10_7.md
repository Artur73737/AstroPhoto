# Windows 0.10.7 / Android 0.4.6

Publication qualification: 24 November 2026.

## Changes

Android now keeps the viewfinder clear of permanent forms. The snapping mode
strip sits below the shutter, with general settings on the left and selected-mode
settings on the right. Landscape uses a side rail. Canon Photo/Sequence and ZWO
native-frame Capture keep distinct workflows. Gallery, focus, stack, native
SDK controls, ROI/format, advanced settings and host tools remain accessible.

There is no front/back camera switch. Canon preview source selection is in Photo
settings. ZWO preview continues independently of disk recording. Stop stays
reachable during acknowledgements; finalization locks incompatible operations.
ZWO geometry changes configure the sensor after the settings acknowledgement.

General settings adds a manual GitHub API update check with installed/latest APK
versions, successful-check time and visible network errors. Manual checks bypass
the automatic daily interval and skipped-version preference. A newer local APK
is never replaced by an older published one. Windows and Android versions are
kept independent. Connection buttons are camera-neutral and acquisition-gated.

Windows 0.10.7 increments the package/application version for this publication;
the camera core is unchanged from 0.10.6. README and website now show actual
Android 0.4.6 screenshots, explicitly captured with no camera connected. Previous
development detail is preserved in the archived README.

## Software checks

- Full Release C++ build, including standalone public headers.
- CTest excluding hardware: **342 passed, 0 failed, 12 optional skipped**;
  354 entries total.
- Android: **32 software tests passed**, including wire authentication, update
  parsing, native units, backend/action safety and actual Compose layout/input.
- Android release lint: **0 errors, 15 warnings** for existing target/dependency,
  backup, wake-lock and launcher-icon metadata. Signed release APK verified using
  the existing publisher identity; versionCode 12, versionName 0.4.6.
- Website: **21 tests passed**, TypeScript check, static production build and
  asset/download export verification.
- MSI packaging verifies x64 dependencies, offline ASTAP catalogs, signed ZWO
  driver catalog and required notices. The runtime root contains ten files;
  project documentation, photos and loose development sources are excluded.
- MSI, APK and release metadata are accompanied by SHA-256 checksums. Public
  upload digests are compared with local files before completion.

## Physical phone and installed host

The signed APK was installed on the connected Pixel 8 Pro. Portrait/landscape,
general/mode sheets and mode navigation were inspected. Two instrumentation
tests passed: Stack layout/navigation, and authenticated pairing over the local
Wi-Fi network with the installed Windows **0.10.6** host without a camera.
Actual state, configuration and control schemas were read; the General-settings
update button made a real GitHub API request. The latter was also checked
manually without PC pairing. No SDK or camera transport was mocked.

The Windows executable and installer were rebuilt as 0.10.7 after this host
integration test; the core/transport implementation is unchanged. No Canon or
ZWO camera was used for this publication. Camera acquisition, lens motion,
optical focus, night-sky stacking and compass orientation were not newly
qualified. Existing experimental limitations remain in their feature guides.

## Packages

- `AstroPhoto-0.10.7-x64.msi`: runtime-only Windows installation, Canon and ZWO
  runtime libraries, ZWO driver, ASTAP/catalogs and app-local VC++ dependencies.
- `AstroPhotoRemote-0.4.6.apk`: signed Android release. Existing signed versions
  upgrade in place. An APK upgrade ends the in-memory pairing session; scan a new
  PC QR to reconnect. Photos remain on the PC.

The MSI itself is not Authenticode-signed. Vendor driver catalog verification
and Android APK signature verification do not constitute an MSI signature.
