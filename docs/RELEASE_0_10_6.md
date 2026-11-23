# Windows 0.10.6 / Android 0.4.5 validation

Publication date: 23 November 2026. This publication was qualified with software
checks only, at the user's request. No camera, USB device, phone, ADB session or
hardware integration test was used during publication qualification.

## Checks

- Full Release C++ build and independently compiled public headers passed.
- Software CTest: **342 passed / 0 failed / 12 optional skipped**, 354 total.
  Hardware-labelled tests were excluded with `-LE hardware`.
- Source/include organization and Canon/ASI SDK boundaries passed.
- Android Debug JVM unit tests: **11 passed / 0 failed / 0 skipped**.
  Coverage includes stream-progress interpolation/clamping, inactive/short
  exposures, acquisition locking and existing protocol/update parsing.
- Android Release lint passed. Signed Release APK build passed.
- APK signing identity matches Android 0.4.4; version code increases from 10
  to 11. The existing signing key was reused.
- MSI payload keeps the ten required runtime root files and dependency/license
  directories. Source guides, example photos and development utilities are not
  installed. The executable remains `AstroController.exe`.
- MSI/APK and release metadata have SHA-256 checksums. Uploaded asset digests
  are checked against local artifacts before completion.

## Behavior

ZWO preview is automatically active while connected and does not save frames.
Start capture saves native stream frames as multi-frame SER or numbered
FITS/PNG/TIFF, with frame/time limits or Until stopped. Stop drains accepted
writes and leaves preview active. Canon single shots and sequences remain
separate and are preserved across backend changes.

For ASI exposures above 0.25 s, Windows and Android show host-estimated per-frame
progress. ASI video does not provide hardware exposure-start/progress events.
The estimate clamps at 100% while waiting for readout and resets on the next
native frame; it is not exposure telemetry or a precision timing measurement.

Android uses a dedicated `feature:zwocapture` module. Configure-and-start is
serialized; Stop remains available while another command acknowledgement is
pending. Native capture/finalization locks camera mutations. Canon-only
capture/Live View switching and Turbo controls are hidden for ASI.

## Remaining validation

This release has not been newly validated with a physical camera or Pixel.
Earlier camera results in `HARDWARE_VALIDATION.md` are historical evidence, not
a substitute for device qualification of the new Android controls. ZWO live
stacking, True North compass orientation and nighttime optical behavior retain
their documented experimental limitations. No automatic recovery of interrupted
SER recordings, ADV or AVI writer is claimed.
