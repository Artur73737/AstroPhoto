# Android 0.4.6 UI validation

Validated on 2026-11-24. Windows remains at 0.10.6; this change is Android-only.

## Navigation and retained controls

- A horizontally scrolling, snapping mode selector sits below the shutter.
- Bottom-left opens general/session settings and phone preview preferences.
- Bottom-right opens the selected mode's parameters. Long forms are sheets,
  rather than permanent content reducing the viewfinder.
- Landscape moves the controls into a side rail. Preview images retain their
  complete field of view.
- Canon modes: Photo, Sequence, Focus, Stack. ZWO modes: Capture, Focus, Stack.
  Camera choices and formats still come from the host's capabilities.
- There is no front/back camera switch. Canon's Live View / Last capture
  selector is inside Photo settings; ZWO keeps the native sensor preview.
- Gallery, native controls, ROI/format, focus tools, stack options, full advanced
  configuration and the host tool catalog remain available.
- The active operation owns Stop. Mode changes and camera mutations stay locked
  during acquisition, including native recording finalization.
- Applying settings preserves drafts until acknowledgement/readback. Applying
  ZWO geometry also invokes the host capture reconfiguration command.

## Automated checks

All 32 software tests passed:

| Area | Tests |
| --- | ---: |
| Mode/backend selection, action safety and finalization | 6 |
| Compose preview bounds, bottom settings placement, tapping/swiping, landscape | 4 |
| Complete disconnected screen rendering and settings routes | 2 |
| Network timing and release selection | 11 |
| Native SDK value/unit conversions | 2 |
| Authentication, wire framing and QR protocol | 7 |

Compose tests run actual layout/input code with Robolectric. They do not replace
or simulate a camera SDK. Release lint completed with **0 errors and 15
warnings**, concerning existing target/dependency versions, backup metadata,
wake-lock timeout and launcher icon resources. The publisher's existing signing
identity was retained and APK signature verification passed.

## Connected phone

The signed release was upgraded in place from 0.4.2 to 0.4.6 (versionCode 12) on
the connected Pixel 8 Pro. The following were inspected over ADB:

- Main portrait viewfinder and removal of the camera-switch icon.
- General settings, Photo/Sequence settings, Focus settings and Stack settings.
- Mode tapping/swiping and matching settings titles.
- Landscape viewfinder, side rail and settings access; return to portrait.
- Installed version and absence of AndroidRuntime errors for this app's process.

Two additional instrumentation tests passed on the signed release APK:

- Stack navigation, disabled capture action without a camera, and both layouts.
- Actual installed MSI host 0.10.6 + Pixel: authenticated local Wi-Fi pairing
  without a camera, real state/configuration/control-schema reads, and the
  General-settings **Check for updates** button using the actual GitHub API.

The update card distinguishes up-to-date, update available and a local version
newer than the public APK. HTTP/network/invalid-release errors remain visible
and retryable. Manual checks bypass the automatic 24-hour throttle and skipped
release preference. Only APK URLs in the configured repository are accepted.

Original phone rotation settings were restored. No camera was exercised for
this refactor: capture, live sensor images, autofocus and stacking require a
separate paired camera session for physical qualification. An APK upgrade ends
the in-memory pairing session; scan a new PC QR to reconnect.

## Local artifacts

- `out/Android/AstroPhotoRemote-0.4.6.apk`
- `out/Android/AstroPhotoRemote-0.4.6.apk.sha256`
- `out/android-046-validation.log`
- `out/android-046-package.log`
- `out/android-046-device/` (phone UI captures)
- `out/android-046-host-validation.log` (installed MSI + real phone + GitHub API)
- `out/android-046-device-layout-test.log` (device navigation/layout)

The APK has been installed locally. This validation does not imply a new GitHub
release or changes to the Windows installer.
