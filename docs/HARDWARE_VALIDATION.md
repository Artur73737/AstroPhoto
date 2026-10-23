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

### Desktop layout, point focus and histograms

- All **57 CTest entries passed** after these changes, including real-camera integration and the opt-in CR2 preview test.
- The real-frame integration test now also computes RGB/luminance histograms and a local focus measurement. A full manual lens-ring sweep with an observed optical peak remains to be verified by an operator.
- Automated focus tests compare sharp/blurred details, reject flat/clipped regions, check image-edge selection, and require a sweep plus stable return before accepting a measured peak. Repeated evaluation of one frame cannot advance the tracker. A context change clears prior measurements.
- Native hidden-window smoke runs passed at 100% on the current work area, at 150% on 1920×1080 and at 200% on 1280×720. Each run checked that all seven native panels were inside the chosen work area, had no pairwise overlap, retained rounded regions and had both icon handles.
- Layout tests additionally cover signed monitor origins and scale factors from 75% to 200%. A captured CR2 preview with its histogram was visually inspected. Simultaneous mixed-DPI physical-monitor dragging and hot-plug recovery still need an operator check; the implementation rebuilds per-monitor fonts and retiles on topology changes.

### Defocus and false focus peaks

- Reproduced two defects before fixing them: a flat patch with deterministic sensor noise scored 37.68 while a sharp edge scored 1.26, and a single score spike permanently raised the stored best. Both regression tests failed on the previous implementation.
- Focus now measures smoothed, noise-gated detail energy without local-variance normalization. Three consistent frames are required to establish a new peak. Invalid low-detail frames retain the confirmed peak but cannot indicate OK.
- All **63 software CTest entries passed**, including progressive edge defocus, a constant-flux star spreading under defocus, noisy flat regions and isolated temporal spikes. Native smoke validation also passed with seven panels and one taskbar entry.
- The hardware suite was attempted after this correction but could not open a session: EDSDK reported no Canon camera detected in all four hardware cases. A physical lens-ring sweep with this version remains unverified; earlier hardware results above describe earlier runs.

## Remaining physical tests

### Power-cycle reconnection and release 0.1.1

- An operator switched the physical EOS 1100D off, waited, and switched it back on during `Hardware.PowerCycleReconnectWithoutRestart`. The same camera object and SDK process reconnected, read properties and opened another clean session. The test passed in 40 seconds. No still exposure was triggered.
- The shutdown callback now marks the device unavailable, and the event pump releases the stale session outside the callback. Handler-removal/close errors from the powered-off device are logged without retaining the old camera reference. Connect also releases a shutdown session defensively.
- All 63 software CTest entries and the regular hardware suite passed on the final build. The interactive power-cycle case requires `ASTRO_HARDWARE_POWER_CYCLE=1` and is otherwise skipped.
- Build artifacts now live under `out/build`; `scripts/package-release.ps1` produces `out/Release` and the x64 MSI. Old build folders are removed only after captured files and settings are preserved.
- The MSI installed and uninstalled successfully for the current user in a custom folder (both exit code 0). Its installed executable passed native UI smoke validation: seven panels, one taskbar entry. Uninstall removed the executable while preserving a user settings file and output sentinel. The installer remembers a custom installation path for maintenance/uninstall.
- Six captured CR2 files were migrated to `out/Release/output`, with SHA-256 verification. Original settings, logs, layouts and the preview test fixture were preserved under `out/preserved`; the portable settings now point to the new output directory. The three old build trees were moved to the Windows Recycle Bin.

### Lens drive and single taskbar entry

- All **60 CTest entries passed**, with `ASTRO_HARDWARE_FOCUS=1` enabled. The physical 1100D reported an attached AF-ready lens and accepted one `Near1` and one `Far1` command in Live View. No still photo was taken. The exact displacement and optical-focus restoration were not measured.
- UI gating tests cover disconnected cameras, inactive Live View, unknown lens state, no lens, MF, unsupported drive and active/paused/stopping captures. The wrapper re-reads lens and AF state before issuing every motor command. Physical AF/MF-switch transitions and medium/large moves remain to be checked with the operator.
- Native smoke validation found seven panels but exactly **one taskbar-eligible window**. Every other panel had `WS_EX_TOOLWINDOW`, no `WS_EX_APPWINDOW`, and the toolbar HWND as owner. Minimize/restore uses native Windows ownership; taskbar interaction remains an operator check.

### Capture and optical checks

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
ctest --test-dir out/build -C Release -L hardware --output-on-failure
```

Close the desktop app first to release its session. Enable Live View testing only when the physical camera is available. The current integration suite does not trigger still exposures.
