# Hardware validation

## Verified on 23 October 2026

Development system: Windows x64, Canon EDSDK 13.19.0.6400, physical Canon EOS 1100D (USB VID_04A9/PID_3217).

- Repeated camera enumeration, session opening, property reads and clean disconnection.
- Readable labels for the current values and all available values returned by this camera.
- PC-only `SaveTo` setting and SDK readback, in three open/close cycles. The previous destination is restored before closing the session. This test does not release the shutter or inspect new card files.
- Ten real 1056x704 JPEG Live View frames, decoded and processed by focus peaking; approximately 7.1–7.5 ms per filtered frame on the development PC.
- Automated software tests for sequences, configuration, peaking, labels, previews and UI layout. English rendering and the red cross reticle were also inspected through native UI smoke screenshots.
- All 53 CTest entries passed after the icon, Stars peaking and zoom changes, including the hardware suite and the opt-in real CR2 preview test. Native smoke rendering passed at 150% scale and checked small/large icon handles on the floating panels.
- Physical 1× → 5× → 10× → 1× Live View transitions. At 1× the JPEG is 1056×704. At both 5× and 10× it is 848×568; the 10× display now crops to 424×284 using the frame metadata. In full-image coordinates (4272×2848), the measured 5× rectangle was `(1708,1138,856,572)` and the 10× rectangle `(1922,1281,428,286)`, with delivered-image origin `(1708,1138)`. The resulting source crop starts at JPEG pixel `(212,142)`.
- Stars peaking processed the real zoom frames without errors. Synthetic tests cover compact/faint stars and rejection of defocus, isolated pixels, clipped cores and straight edges. This is not yet night-sky optical-focus validation.

The zoom correction follows section 5.2.59 of the Canon EDSDK 13.19 API reference: a 10× request returns a 5× USB image. The Canon-authored reference was read from [this pinned SDK mirror](https://raw.githubusercontent.com/shucai-ok/Cannon_EDSDK/ef05251e6292dea66f6a113bf6adcd3826348aaa/Document/EDSDK_API_EN.pdf).

SDK artifact provenance and SHA-256 hashes are in `external/edsdk-manifest.json`. Camera firmware and lens details have not been recorded. These results do not establish full validation of every capture mode.

## Remaining physical tests

1. Record camera firmware and lens model. Confirm the SDK compatibility list for other camera models before claiming support.
2. Exercise missing USB devices, EOS Utility contention, multiple cameras, power-off and unplugging during idle and transfer. Check visible errors and resource cleanup.
3. Compare ISO/Av/Tv/WB/image-quality labels with the camera display. Write and read back supported settings, including modes controlled by the physical dial.
4. Capture RAW, JPEG and RAW+JPEG. Check file count, integrity, EXIF values and transfer acknowledgements. Verify both PC-only and PC+card storage on actual captures. Confirm no new card file is created in PC-only mode and existing images remain unchanged.
5. Test 2-second and 30-second Bulb captures using the supported Tv/AE/command combination. Verify duration from EXIF; file size is not proof of exposure time.
6. Test stop, timeout, app closure and USB failure during Bulb. Physically confirm shutter closure after an SDK failure.
7. Check zoom framing at additional positions and lens-drive focus with compatible lenses. Check both peaking modes against known focused/defocused subjects, real stars and noise levels; they are not calibrated optical-focus telemetry.
8. Exercise low disk space, unwritable folders, Unicode paths, name collisions and download write failures. Original files must remain intact and incomplete files distinguishable.
9. Verify save-destination restoration after normal disconnect and errors, including a missing/full card when PC+card is selected.
10. Before adding RAW export, verify which development APIs are actually supported by the SDK version. Preserve original CR2 files.

## Not implemented

Camera-only saving, RAW-to-TIFF/JPEG export, individual PictureStyle parameters, EVF focus/zoom positioning, Bulb telemetry and EXIF-based automated capture validation.

## Running the existing tests

```powershell
./scripts/build.ps1 -WithEDSDK -HardwareTests
$env:ASTRO_HARDWARE_TESTS = '1'
$env:ASTRO_HARDWARE_LIVEVIEW = '1'
ctest --test-dir build-canon -C Release -L hardware --output-on-failure
```

Close the desktop app first to release its session. Enable Live View testing only when the physical camera is available. The current integration suite does not trigger still exposures.
