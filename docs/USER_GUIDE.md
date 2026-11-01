# AstroPhoto user guide

AstroPhoto runs on Windows and controls the USB-connected Canon. AstroPhoto Remote
is an Android controller for that running application. Original captures and stack
results are saved on the PC; the phone receives previews, not a second photo library.

## Install and check versions

Download the Windows MSI and Android APK from the [website](https://artur73737.github.io/AstroPhoto/#downloads)
or [GitHub Releases](https://github.com/Artur73737/AstroPhoto/releases).
**Each application has its own version.** A GitHub release groups the packages;
its tag is not the Android version. For example, release v0.7.0 contains Windows
0.7.0 and Android 0.2.0. Check the APK filename and Android app information.

The MSI includes the Canon runtime and app-local Visual C++ dependencies. The
default installation is per-user. The Android release APK is signed with the
project key. Install it over an older release signed with the same key. A debug
APK has a different signature; uninstall that debug build before installing the
release if Android reports a package conflict.

For an integrity check, run PowerShell with the actual downloaded filename:

```powershell
Get-FileHash .\AstroPhotoRemote-0.2.0.apk -Algorithm SHA256
```

Compare all 64 hexadecimal characters with the corresponding asset's checksum.
Do not compare an APK hash with the MSI hash. The website links to original GitHub
assets and refreshes its catalog; if GitHub is unavailable, it identifies the
saved catalog as a fallback rather than presenting it as a successful live check.

## First capture on Windows

1. Connect and power on the camera. Close EOS Utility and other camera controllers.
2. Open AstroPhoto and connect. The EOS 1100D is the physically tested module.
3. Select exposure settings in the camera panel. Lists reflect the connected
   camera's available values. For stacking, use M, RAW or RAW+JPEG and fixed ISO.
4. Choose the output root, session name and save destination. **PC only** downloads
   to the PC; **PC and camera** also keeps a camera-card copy.
5. Use Live View to frame the scene and check focus. Take a single capture to
   inspect the actual exposure before starting a long sequence or stack.

New captures are organized under the capture date and user session name, with
subfolders for their type. A live-stack run has its own directory containing
original frames, the final FITS and a JSON report. See the exact layout in
[RAW live stacking](LIVE_STACKING.md). Preview stretch and the Windows Night theme
do not change saved image data.

## Phone pairing and everyday controls

1. Keep the PC and phone on a reachable local network.
2. On the PC, open **Settings → Android / WebSocket**, choose the interface and
   start the server. Use the PC's reachable address, not a virtual adapter.
3. On Android, open **Connection → Scan PC pairing QR** and scan the displayed QR.
4. The QR is single-use and expires after 120 seconds. Successful authentication
   closes the PC dialog and displays the connected device name.

The Android foreground service maintains the session when switching apps. Task
removal or a process kill ends it. The pairing key is not saved for a future app
launch. Generate a fresh QR after changing networks. **New QR** restarts the
server and disconnects an existing controller. A lost phone connection does not
cancel a sequence already owned by the PC.

The phone exposes Live View Start/Stop, camera settings, capture, sequences,
focus and live-stack controls. Values are confirmed from PC/camera state.
Conflicting actions lock while the camera is busy; a submitted command is not
proof that exposure or autofocus has completed. Photos are not saved on Android.

Pair only on a trusted network. The connection is authenticated but uses
unencrypted WebSocket transport. Do not forward its port to the internet.
See [Android remote](ANDROID_REMOTE.md) for lifecycle and protocol details.

## Focusing without confusing the tools

| Tool | What it does | What its result means |
| --- | --- | --- |
| Measure point | Measures a selected region without moving the lens | A relative sharpness measurement, not an AF confirmation |
| Auto focus point | Places the camera AF area and runs camera autofocus | OK only after camera confirmation; low contrast may fail |
| Near / Far | Drives an AF-enabled lens with Small, Medium or Large commands | A motor command, not an absolute angle or distance |
| Normal peaking | Highlights strong local edges | A visual focus aid, not a guarantee of correct focus |
| Stars peaking | Selects compact star-like sources | Candidate detections; noise and saturation affect selection |
| Stellar autofocus | Searches and refines Large → Medium → Small | Check the final state and any precision warning |

A motor-driven attached lens must be switched to AF. Point autofocus and stellar
autofocus serve different subjects. A warning about small-step resolution means
the measurements could not establish that extra precision; it is not evidence of
a perfect focus. Review a real exposure as well as the preview.

The 1100D supplies a short-exposure Live View feed. Setting Tv to 30 seconds does
not integrate a 30-second video frame. If stars are faint only in Live View,
inspect a captured exposure before changing detection thresholds. Do not treat
noise that appears after brightening a preview as additional real stars.

## Capture a live stack

1. Set exposure and output preferences before starting. The stack does not
   silently change ISO, aperture, shutter or focus.
2. Select **Live stacking** in the Image panel. On Android use the stack controls.
3. Choose a preset, custom frame count or **Until stopped**. The frame target
   counts shutter releases, including frames later rejected by processing.
4. Enable **Align stars** for a star field. Without it, images accumulate at fixed
   pixel coordinates and sky motion is not corrected. Keep it off for a stationary
   indoor processing test where a star registration has no meaning.
5. Leave a sensible delay between captures: default 3 seconds, minimum 2. The
   pipeline completes download and processing before waiting for the next shot.
6. Watch accepted/captured/rejected counts. Use **Stop** to finish the current
   exposure, download and processing and save the partial result.

For N frames of exposure E and gap D, exposure plus inter-frame gaps take at least
`N × E + (N − 1) × D` seconds. Downloads and RAW processing add time. The website's
calculator estimates this lower bound; it does not control the camera. Above
0.25 seconds, the exposure progress bar shows host timing, not transfer completion.

Low/Mid/High affect display only. The saved stack is linear RGB FITS, **not** a
synthetic Canon CR2. Keep the original CR2 frames for later calibration and
reprocessing. Optional hot-pixel and horizontal-banding correction are cosmetic
processing tools, not substitutes for bias/dark/flat calibration. Banding
correction may alter real horizontal structure; keep it off unless needed and
inspect the result at conservative settings.

The [stack reference](LIVE_STACKING.md) documents the colour pipeline, per-pixel
coverage, rejection, file structure, configuration limits and JSON diagnostics.
The [hardware report](HARDWARE_VALIDATION.md) distinguishes indoor camera validation
from real-sky registration, which has not been physically validated for this release.

## Troubleshooting

| Symptom | Check first |
| --- | --- |
| APK version looks different from the release tag | Read the APK filename; Windows and Android versions are independent |
| Android refuses an update | Check for a debug/release signature conflict; do not uninstall a normal same-key release unnecessarily |
| QR pairing fails | Refresh an expired QR, confirm the PC interface, reachable network and firewall access |
| Phone disconnects after app removal | Task removal ends the foreground service; reopen and pair again |
| Camera is not available | Power, USB cable and competing software; inspect the camera log before retrying |
| Live View stars are too dim | Inspect a real captured exposure; Live View does not use a long Tv as its frame duration |
| Focus controls are disabled | Lens AF switch, supported lens, current capture/AF operation and Live View state |
| Stack has fewer accepted frames than shots | Read rejection reasons in the log/report; do not disable star alignment merely to bypass failed registrations |
| FITS looks dark in another viewer | Apply a display stretch to the linear data; preview levels are not baked into the FITS |
| Horizontal structures change | Disable banding correction or reduce its strength and maximum offset |

After a camera/USB error, recovery does not automatically repeat shutter commands
or resume interrupted capture. Review the operation state and files already saved
before starting another run. For a bug report include application versions,
camera/lens, relevant logs and the stack JSON when applicable. Remove personal
paths or device names before posting logs publicly.

## Further references

- [Android controller and networking](ANDROID_REMOTE.md)
- [RAW stacking and processing](LIVE_STACKING.md)
- [Physical hardware validation](HARDWARE_VALIDATION.md)
- [Native MCP control API](MCP.md)
- [Build and run](../README.md#build-and-run)
