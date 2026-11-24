# Android 0.4.7 — Canon Sequence

Validated 24 November 2026 with the signed release APK (versionCode 13).

Canon keeps Photo, Sequence, Focus and Stack; ZWO keeps Capture, Focus and Stack.
Sequence mode settings now show the plan directly, before the shared camera
controls. Rows expose frame type, count, output format where supported, Bulb,
exposure, initial delay, delay and interval, followed by camera-reported choices.
Add/Duplicate/Remove and row selection edit a draft; Apply waits for host readback.
Editing does not start a capture. Advanced configuration and host tools remain.

All 33 software tests passed, including Compose row editing, backend selection,
action gating, layouts, native values, release parsing and wire protocol. Release
lint passed with 0 errors and the existing 15 warnings. The publisher signing
identity was reused.

The APK was installed on Pixel 8 Pro. The real `CanonSequenceRemoteTest` paired
over Wi-Fi with the installed Windows 0.10.8 host and physical Canon EOS 1100D,
received a real preview, opened Sequence settings, added/duplicated/removed a
row, applied the plan and verified its acknowledgement/readback. The original
plan was restored and verified. No photographs or motor-focus commands were
issued by this test. No SDK or transport was mocked.

See [Windows/Android release qualification](RELEASE_0_10_8.md) and the separate
hardware and phone sequence logs published with the release.
