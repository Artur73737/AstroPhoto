# AstroPhoto

![AstroPhoto desktop workspace with Canon EOS 1100D Live View and focus peaking](https://raw.githubusercontent.com/Artur73737/AstroPhoto/main/docs/image.png)

[Open the full-size application preview](https://raw.githubusercontent.com/Artur73737/AstroPhoto/main/docs/image.png)

A Windows astrophotography controller for Canon EOS cameras, developed and tested with a **Canon EOS 1100D**. Written in C++20 with Dear ImGui, GLFW, OpenGL and Canon EDSDK.

The interface is a compact desktop workspace: independently movable native Windows panels, rounded corners, a central image viewer, and dedicated camera, sequence, output and log panels. All controls are rendered by Dear ImGui.

## Features

- **Single captures and sequences** with Light, Dark, Flat and Bias frame types.
- **Per-row exposure settings**, initial delay and a separate interval between captures.
- **PC-only saving**, with an optional **PC + camera card** destination.
- **Readable camera settings**: ISO numbers, f-stops, shutter times, exposure modes, white balance, picture styles and image quality. Available options are queried from the camera.
- **Live View** with camera zoom and lens focus controls.
- **Magenta focus peaking**, an on/off toggle, adjustable threshold and a **Stars** mode for compact, round sources.
- **Red reticles**: cross, open cross, circle, target and rule-of-thirds grid.
- **Click-to-measure manual focus** with a local sharpness score, measured peak and a return-to-best indication.
- **Lens motor − / + controls** beside Focus point, gated by detected lens and AF state, with three Canon step sizes.
- **RGB and luminance histograms** over Live View and the latest captured-image preview.
- **Latest-capture preview** for JPEG and embedded CR2 previews, decoded asynchronously.
- **Italian and English UI**, dark/light themes and adjustable UI scaling.
- A minimal SVG application icon, embedded as a multi-resolution Windows icon with per-monitor DPI selection.
- Persistent JSON preferences, automatic desktop tiling and an operation log.

## Current validation status

| Area | Status |
| --- | --- |
| Windows x64 build and native ImGui panels | Built and tested |
| Canon EOS 1100D USB connection | Tested with a physical camera, including repeated open/close cycles |
| Camera settings | Current values and available choices read from the physical camera |
| Live View and peaking | Tested on 10 real 1056 × 704 frames; filter time approximately 7 ms/frame on the development PC |
| Live View 1× / 5× / 10× | Physical 1100D transition test, including the additional crop required for Canon's 10× USB preview |
| Stars peaking | Synthetic sharp/defocused/noise fixtures and real-frame processing; night-sky focus validation remains outstanding |
| Point focus and histograms | Automated local measurements and focus-sweep tests; processing checked on real 1100D Live View frames |
| Lens motor controls | Physical AF-lens detection and accepted Near1/Far1 commands on the 1100D; AF/MF UI gating tested |
| Desktop layout and DPI | Native non-overlap/bounds checks at 100%, 150% and 200%; signed monitor origins covered by layout tests |
| PC-only destination | Explicit SDK setting with readback verification before capture |
| Sequence timing, pause/stop, retries and configuration | Automated domain tests |
| JPEG/CR2 preview | WIC/LibRaw decoding; real CR2 fixture supported by an opt-in test |
| Still capture, Bulb and focus accuracy | Implemented; complete physical capture/optical-focus validation remains outstanding |

This is an actively developed application. See [hardware validation](docs/HARDWARE_VALIDATION.md) for remaining checks and known gaps.

## Requirements

- Windows 10 or 11, x64.
- Visual Studio 2022 with **Desktop development with C++** and the Windows SDK.
- CMake 3.24 or newer, Git and PowerShell (PowerShell 7 recommended).
- A USB-connected Canon camera supported by the installed EDSDK version.
- Canon EDSDK headers, x64 import library and matching runtime DLLs for camera control.

Close EOS Utility and other camera-control programs before connecting. The current application expects one connected Canon camera.

## Build and run

```powershell
git clone https://github.com/Artur73737/AstroPhoto.git
cd AstroPhoto
```

### Build with camera support

Place your Windows EDSDK package under `external/EDSDK` with this structure:

```text
external/EDSDK/
├── EDSDK/Header/
│   ├── EDSDK.h
│   ├── EDSDKTypes.h
│   └── EDSDKErrors.h
└── EDSDK_64/
    ├── Library/EDSDK.lib
    └── Dll/
        ├── EDSDK.dll
        └── EdsImage.dll
```

The local development build uses **EDSDK 13.19.0.6400 x64**. Its exact source URLs and SHA-256 hashes are recorded in [the SDK manifest](external/edsdk-manifest.json). To reproduce that download from the recorded third-party mirror:

```powershell
./scripts/install-edsdk.ps1
```

This is a public mirror, **not a Canon-hosted download**. The hashes identify the downloaded files; they do not establish publisher authenticity. The runtime DLLs are not Authenticode-signed. The official SDK is available through the [Canon Developer Programme](https://developers.canon-europe.com/s/camera). Canon SDK terms and original notices apply. SDK binaries, headers and downloaded third-party source trees are excluded from this repository.

Build and launch:

```powershell
./scripts/build.ps1 -WithEDSDK
./scripts/run.ps1 -Connect
```

`build.ps1` fetches the pinned open-source dependencies, builds the app, runs software tests and checks module boundaries. CMake copies `EDSDK.dll` and `EdsImage.dll` next to the executables. `-Connect` opens the camera session on launch; it does not trigger a capture.

Executable: `build-canon/Release/AstroCanonController.exe`.

### Build the editor without Canon EDSDK

```powershell
./scripts/build.ps1
./build/Release/AstroCanonController.exe
```

This build supports editing plans and preferences. Camera controls are disabled explicitly; it does not emulate a camera or generate sample photographs. The run script prefers the camera-enabled executable when both builds exist.

### Manual CMake build

```powershell
./scripts/bootstrap.ps1
cmake -S . -B build-canon -G "Visual Studio 17 2022" -A x64 -DASTRO_WITH_EDSDK=ON
cmake --build build-canon --config Release --parallel
ctest --test-dir build-canon -C Release --output-on-failure -LE hardware
```

Use `EDSDK_ROOT` to specify another SDK package location. Header, import-library and runtime paths can also be set individually; see [FindEDSDK.cmake](cmake/FindEDSDK.cmake).

## Using the application

### Connect and configure

1. Turn on the camera and connect it by USB.
2. Click **Connect** in the Connection panel.
3. Select ISO, aperture, shutter, white balance and image quality in the Camera panel. Read-only settings remain visible; some modes require the physical mode dial.
4. Use **Refresh** after changing settings on the camera. Property changes are not yet refreshed automatically through SDK property events.

### Choose where captures are saved

In **Camera → Save destination**, or the **Files and session** panel, choose:

- **PC only** (default): sends new captures to the configured PC folder without requesting a copy on the camera card.
- **PC + camera card**: requests both the host download and a card copy. A usable card is required.

The selection applies when starting the next single capture or sequence. The wrapper sets Canon's `SaveTo` property and reads it back before proceeding; a mismatch stops preparation. The previous camera destination is restored when the session closes normally. Existing card images are never deleted.

Set the output folder and session name in **Files and session**. Captures are organized as:

```text
<output folder>/<session name>/<Light|Dark|Flat|Bias>/<capture filename>
```

Names include UTC time, a counter and selected camera-setting codes. Original CR2/JPEG files are retained. Exclusive file creation prevents existing files from being overwritten.

### Single capture

Choose a frame type and optional initial delay, then click **Capture**. A single capture runs independently of the sequence rows. Dark frames require covering the lens. The latest downloaded photo appears in the central viewer.

### Sequences

Add rows, choose their frame type, count, exposure settings, initial delay and interval.

- **Initial delay** applies before the first capture of a row.
- **Interval** starts after download completion and applies before the next capture, including across row boundaries.
- When both delays apply, the later deadline wins; delays are not added together.
- **Pause** completes the current capture and pauses before the next one.
- **Stop** attempts to end an active Bulb exposure and closes an active capture session to isolate late downloads. Reconnect before continuing after cancellation.

For ordinary captures, shutter time is controlled by the camera's **Tv** setting. The row's non-Bulb time value is an exposure timeout budget, not a shutter command. For Bulb, choose a compatible Bulb Tv setting and enable Bulb in the capture settings; the duration then controls the requested exposure time.

### Live View, peaking and reticles

Select **Live View**, then **Start Live**. Use the supported 1×/5×/10× camera zoom and lens-focus controls.

The selected zoom reflects the value reported by the received frame. Canon EDSDK 13.19, reference section 5.2.59, specifies that a 10× request still delivers a 5× image over USB. AstroPhoto uses the frame's zoom rectangle and image origin to crop that image by another factor of two. On the tested 1100D, both zoom modes deliver an 848 × 568 JPEG; 5× displays the full frame and 10× displays the appropriate 424 × 284 region. This matches the requested framing without adding sensor detail.

Enable **Focus peaking** beside Live View to highlight high-contrast edges in magenta. A lower threshold shows more edges; a higher threshold keeps stronger edges. Peaking is a visual aid rather than a calibrated guarantee of perfect focus. Confirm fine detail using camera zoom.

Enable the adjacent **Stars** toggle to look for compact, approximately round light sources against the local background. Magenta rings leave their centers visible. This mode filters isolated hot pixels, broad or elongated sources and clipped cores; lower the exposure if bright stars are saturated. The threshold controls detection sensitivity. It has automated coverage for compact stars, defocus, faint sources and rejection cases, but still needs comparison with real focused/defocused night-sky images.

Enable **Crosshair** and select a shape from its adjacent menu:

- Cross
- Open cross, leaving the center unobstructed
- Circle
- Target with concentric circles
- Rule of thirds

All reticles are red and clipped to the image area. Peaking and reticles affect only the displayed image; downloaded files are unchanged.

### Manual focus on a selected point

In Live View, enable **Focus point**, then click a detail or star in the image. The outlined region measures up to 64 × 64 preview pixels around the click. Slowly turn the lens focus ring through the sharpest point until detail softens again, then return toward the measured peak.

When **Focus point** is enabled, the adjacent **− / +** buttons drive the lens motor toward nearer focus / infinity. Select **1 – Smallest**, **2 – Medium** or **3 – Large**. These are the three relative Canon EDSDK drive sizes, not degrees or exact multiples of the smallest step. The motor moves the internal focusing mechanism; it does not necessarily rotate the external ring. There is no automatic focus search or shutter release.

Buttons require an attached electronic lens reported in AF mode, active Live View and no capture in progress. Put the lens switch on **AF** for motor control. MF, absent lenses, unknown AF state and a rejected unsupported drive disable the controls. The worker polls lens state, and the SDK wrapper checks it again immediately before every move. Each click sends one command; ambiguous or busy movements are not retried automatically. Manual ring adjustment and point-focus measurement remain available when motor control is disabled.

The overlay reports current sharpness, the confirmed peak and peaking coverage. Sharpness measures smoothed detail energy above an estimated noise floor; it is not divided by local contrast, which can collapse under defocus and incorrectly boost noise. A new peak requires three consistent frames, so move the ring slowly and briefly hold each candidate position. A single spike cannot set the peak. Low-detail frames preserve the previous peak but cannot report OK.

After a sufficiently varied sweep and several stable readings within 5% of the measured maximum, the overlay turns green with **OK: near the best measured focus**. A stationary image alone cannot trigger OK. Low detail, significant clipping or a stale stream prevents confirmation. Keep exposure and lighting steady during comparison. Use **Reset focus** when reframing or changing the subject; clicking another point, changing zoom or reported camera settings also resets the comparison. The region stays at the selected image coordinates; it does not track moving subjects.

This is a relative manual-focus aid, not absolute optical-focus telemetry. Noise, seeing, subject movement and exposure changes can affect it. Peaking coverage reports the selected filter's response; it is not a percentage of optical focus accuracy.

**ROI coverage** counts the pixels highlighted by peaking inside the selected region. With **Focus peaking** off, the overlay explicitly says **Peaking: off**. With peaking enabled, zero means no pixels passed the chosen filter inside that region: try a detailed edge and a lower threshold. Use **Stars** only for compact stellar sources; it can correctly return zero on ordinary subjects or saturated stars. The overlay shows two decimal places and a pixel count so sparse detections are not rounded to zero. Local sharpness and the return-to-best indication work independently of peaking coverage.

### Histograms

**Histograms** enables a compact overlay at the top right of both Live View and the latest capture. It plots red, green, blue and display luminance from black to white with a logarithmic count scale. Measurements use the image before peaking, reticles and other overlays. A CR2 histogram describes its embedded preview, not the original RAW sensor data. At 10×, the Live View histogram describes the displayed crop.

### Preferences

Each launch tiles all panels inside the **primary monitor's work area**, excluding the taskbar. Older saved positions do not override this startup arrangement. The image occupies the central area, camera controls sit on the left, sequences on the right, and output/log panels below. **Windows → Reset layout** restores the arrangement. Monitor connection, resolution, work-area, DPI and UI-scale changes also trigger retiling. Panels can be moved to other monitors during the session; each monitor DPI has its own rasterized font in the shared atlas and native icon sizes.

After a move or resize ends, overlapping or out-of-bounds floating panels are retiled on their current monitor. Panels that already fit without overlap keep their positions. This keeps secondary screens usable without moving their panels back to the primary monitor during ordinary dragging.

The Windows taskbar has **one AstroPhoto entry**, belonging to the main toolbar. Floating panels are owned tool windows and do not create extra taskbar buttons. Minimizing/restoring the owner groups its panels while preserving independent desktop placement.

Open **Settings → Language → Italiano / English** to switch the UI language. Theme and UI scale are in the same menu. **Save preferences** persists these options, the peaking settings, save destination and reticle selection.

| File beside the executable | Purpose |
| --- | --- |
| `defaults.json` | Shipped defaults, used when no personal configuration exists |
| `user.json` | Saved preferences and capture plan |
| `desktop-layout.ini` | ImGui session state; window positions are retiled at the next launch |
| `astro.log` | Timestamped operation and error log |

Relative output paths are resolved against the executable directory. Run from a writable location. Backend/OS diagnostic messages retain their original language in the log.

## Architecture

```text
src/
├── core/             Domain models, interfaces, sequencing and file organization
├── configuration/    JSON loading, validation and atomic saving
├── edsdk_wrapper/    Canon SDK ownership, properties, events and transfers
├── infrastructure/   Filesystem, clock and camera worker
├── ui/               Dear ImGui panels, translations and reticles
└── app/              Native host, rendering, preview decoding and focus peaking
```

- `core` has no EDSDK, ImGui or Windows dependency.
- Only `edsdk_wrapper/src` includes Canon SDK headers.
- SDK initialization, camera calls and callbacks belong to one STA worker with COM and a Windows message pump.
- The UI exchanges commands and snapshots through a synchronized mailbox.
- RAII releases Canon resources; callback exceptions are caught and reported.
- Host downloads are acknowledged with `EdsDownloadComplete` after successful file writing and flushing.
- Busy retries are bounded and restricted to idempotent configuration work. Ambiguous shutter commands are not retried automatically.

## Testing

Software tests use domain test doubles only; they are never linked into the production app. Hardware tests use the real SDK and a physical camera.

```powershell
ctest --test-dir build-canon -C Release --output-on-failure -LE hardware
./scripts/check-boundaries.ps1
```

Enable physical-camera tests explicitly:

```powershell
./scripts/build.ps1 -WithEDSDK -HardwareTests
$env:ASTRO_HARDWARE_TESTS = '1'
$env:ASTRO_HARDWARE_LIVEVIEW = '1'
ctest --test-dir build-canon -C Release -L hardware --output-on-failure
```

Close the app before running hardware tests. These tests open camera sessions, read properties, verify PC-only destination configuration and optionally acquire/process 10 Live View frames. They do not release the shutter.

With Live View enabled, the suite also requests 1× → 5× → 10× → 1×, waits for each frame to confirm the requested zoom, checks the display crop and processes the result through Stars peaking.

Lens-state reading is included in the hardware suite. To additionally test motor movement, set `$env:ASTRO_HARDWARE_FOCUS = '1'` and run `astro_hardware_tests.exe --gtest_filter=Hardware.LensFocusStateAndOptionalDrive`. With an AF-ready lens, this sends one smallest near movement and one smallest far movement; equal opposite commands do not guarantee exact focus restoration. Otherwise it verifies that movement is rejected. It does not take a photograph.

Optional real-file CR2 preview test:

```powershell
$env:ASTRO_PREVIEW_CR2 = 'C:/path/to/sample.CR2'
ctest --test-dir build-canon -C Release -R PhotoPreview --output-on-failure
```

Native UI smoke tests render hidden windows, validate their rounded regions and write panel PNGs and `desktop-smoke.txt` beside the executable:

```powershell
./build-canon/Release/AstroCanonController.exe --smoke-test --smoke-language=en
./build-canon/Release/AstroCanonController.exe --smoke-test --smoke-dpi=1.5 --smoke-width=2100 --smoke-height=1350
```

Smoke tests do not connect to the camera or change personal layout/preferences.

### Application icon

The editable source is [assets/astrophoto.svg](assets/astrophoto.svg). The checked-in ICO contains individually rendered 16, 20, 24, 32, 40, 48, 64, 96, 128 and 256 pixel images. CMake embeds it in the executable; native panels select small and large icons for their current monitor DPI. The existing PerMonitorV2 manifest handles UI DPI awareness.

To regenerate the ICO after editing the SVG, install Python packages `PyMuPDF` and `Pillow`, then run `python scripts/build-icon.py`. These packages are only needed to regenerate the asset, not to build or run the application.

## Known limitations

- Complete still-capture, RAW+JPEG transfer, Bulb timing and interrupted-exposure hardware validation remains outstanding.
- No RAW-to-TIFF/JPEG export. CR2 viewing uses the embedded preview through LibRaw.
- No camera-only destination, individual PictureStyle parameters, EVF focus/zoom positioning, Bulb telemetry or EXIF exposure verification.
- Camera selection is currently limited to a single connected device.
- Changing UI language does not translate arbitrary SDK, filesystem or operating-system error text.

## Dependencies and notices

| Dependency | Pinned version | License / source |
| --- | --- | --- |
| Dear ImGui | 1.91.9b-docking | MIT, [ocornut/imgui](https://github.com/ocornut/imgui) |
| GLFW | 3.4 | zlib, [glfw/glfw](https://github.com/glfw/glfw) |
| nlohmann/json | 3.11.3 | MIT, [nlohmann/json](https://github.com/nlohmann/json) |
| GoogleTest | 1.16.0 | BSD-3-Clause, [google/googletest](https://github.com/google/googletest) |
| LibRaw | 0.22.2 | CDDL-1.0 option, [LibRaw/LibRaw](https://github.com/LibRaw/LibRaw) |
| Cousine font | ImGui distribution | Apache-2.0 |
| Canon EDSDK | 13.19.0.6400 used locally | Canon SDK terms; see the manifest and retained package notices |

Public dependency commits are pinned in `scripts/bootstrap.ps1`. Preserve the upstream license and copyright files when distributing dependencies. Canon property-label tables are derived from the SDK sample tables and retain source attribution.
