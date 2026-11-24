# Windows 0.10.8 / Android 0.4.6 — Canon reconnection fix

Qualification date: 24 November 2026.

## Fixed

After a failed Canon connection, the camera router retained the old SDK owner.
On retry, assigning a newly constructed backend initialized EDSDK before the old
backend was destroyed. Two overlapping SDK lifetimes could leave Canon stuck
in `EDS_ERR_INTERNAL_ERROR (0x2)` until AstroPhoto was restarted.

The router now destroys the disconnected backend before constructing another
one. Failures during backend setup, module selection, discovery or session open
also release the backend immediately. The same lifetime ordering applies when
switching between Canon and ZWO; a connected backend is not replaced.

## Actual Canon hardware

Tests use the physical Canon EOS 1100D and real Canon SDK, without mocks:

- Three consecutive failed backend setups after actual SDK construction,
  followed by a successful 1100D connection in the same router/process.
- Three session open/read-properties/close cycles. Actual ISO, Av, Tv, exposure
  mode, white balance, picture style and image-quality choices were read.
- Ten real **1056 × 704** Live View frames, JPEG decoding, peaking, histogram
  and focus-measurement processing.

All three targeted hardware tests passed. The corrected desktop application
also opened the real camera and enabled Live View. No photographs or lens-drive
commands were issued; optical AF accuracy and night-sky stacking were not tested.
No manual USB unplug/power-cycle was performed in this qualification.

The new MSI was installed locally with exit code 0. The installed executable
reports 0.10.8 and matches the packaged executable's SHA-256; existing user JSON
preferences were preserved byte-for-byte. The installed application then
connected to the physical 1100D and enabled Live View. The hardware test log
is published with the release artifacts.

## Builds and packages

- Full Release C++ build with public-header compilation and boundary checks.
- Software CTest: 342 passed, 0 failed, 12 optional skipped (354 entries).
- Android 0.4.6 is unchanged functionally and rebuilt for this bundle: 32
  software tests, release lint and existing-key APK signature verification.
- MSI retains `AstroController.exe`, runtime dependencies, ZWO driver and ASTAP
  catalogs/notices; project photos and loose development files are excluded.
- MSI administrative extraction and installed executable version are checked.
- Uploaded MSI/APK and metadata digests are compared with local SHA-256 values.

The Android UI/pairing qualification remains in
[Android 0.4.6 validation](ANDROID_UI_0_4_6.md). APK upgrades still require a fresh
QR because pairing credentials remain in RAM. Windows and APK version numbers
are independent. The MSI itself is not Authenticode-signed.
