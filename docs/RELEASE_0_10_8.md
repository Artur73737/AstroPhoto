# Windows 0.10.8 / Android 0.4.7 — Canon reconnection and Sequence panels

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

Canon Sequence is restored as a dedicated region in automatic compact/medium
layouts instead of sharing the Log tab. It reopens on camera connection and has
an explicit Open sequence editor menu command. Custom layouts remain editable;
ZWO keeps its dedicated native-frame Capture workflow. Backend identity, rather
than incidental optional capture metadata, selects the Canon/ZWO editor.

Android 0.4.7 keeps the Canon Sequence mode and now shows its capture-plan editor
directly, with Add/Duplicate/Remove row controls and all row timing/exposure
parameters. Drafts apply only after acknowledgement; editing does not start a shot.

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

The signed Android 0.4.7 APK was installed on the connected Pixel 8 Pro. A real
LAN test paired with the installed MSI host and physical Canon, received its
Live View image, selected Sequence, invoked Add/Duplicate/Remove/Apply, verified
the new host plan and restored the original plan. This test passed without
issuing exposures. Its log is included with the artifacts. The sequence plan
is shown first, followed by the existing camera/Live View/Turbo controls.

## Builds and packages

- Full Release C++ build with public-header compilation and boundary checks.
- Software CTest: 343 passed, 0 failed, 12 optional skipped (355 entries),
  including Sequence visibility, dock separation and reconnection reopening.
- Android 0.4.7: 33 software tests, including visible sequence row editing,
  release lint and existing-key APK signature verification; versionCode 13.
- MSI retains `AstroController.exe`, runtime dependencies, ZWO driver and ASTAP
  catalogs/notices; project photos and loose development files are excluded.
- MSI administrative extraction and installed executable version are checked.
- Uploaded MSI/APK and metadata digests are compared with local SHA-256 values.

Earlier Android UI/pairing qualification remains in
[Android 0.4.6 validation](ANDROID_UI_0_4_6.md). APK upgrades still require a fresh
QR because pairing credentials remain in RAM. Windows and APK version numbers
are independent. The MSI itself is not Authenticode-signed.
