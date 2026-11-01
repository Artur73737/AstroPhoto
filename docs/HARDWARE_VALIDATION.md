# Hardware validation

## 1 November 2026: RAW live stacking (Windows 0.7.0 / Android 0.2.0)

- Final native-MCP run: **5 exposures / 5 accepted / 0 rejected**, alignment off,
  horizontal banding on (1.0 strength, 32-row radius, 0.02 limit), unchanged
  ISO 800 / f/9 / 1 s. FITS + five CR2s + JSON verified. Correction RMS per frame
  0.00021–0.00023; no row reached the correction limit.
- All 265 Windows tests passed. A separate GPU smoke test verified full-client
  red output; MCP confirmed that Night mode leaves the coloured stack endpoint
  byte-identical. Histograms/levels now sit in the stack controls rather than
  over the image. The Windows-only theme is Dark / Light / Night.

- Stack mode lives inside Image, with controls to the left of the shared preview.
  Responsive layout tests retain the six existing docked panels.
- Real Canon EOS 1100D / Pixel 8 Pro validation passed: three CR2 captures,
  accumulated binary preview, display levels, fixed limit, graceful infinite Stop,
  portrait/landscape, unchanged camera properties and Live View restoration.
  A second five-frame run also accepted all five frames with alignment disabled.
- The corrected-colour run used the camera's existing ISO 800, f/9 and 1 s.
  Frozen RAW white balance was 2533/1024/1400/1024. Per-frame isolated CFA
  corrections and output dimensions (4290 × 2856) were checked in the JSON report.
  FITS files contain float linear sRGB and a coverage extension; original CR2s
  remain untouched. Both desktop and phone previews were inspected.
- A deliberately conflicting property command was refused on the PC while
  stacking. The phone's Live View toggle was tested off/on, including waiting
  through multiple polls to ensure it did not automatically restart.
- These are indoor acquisition/colour tests, not night-sky registration evidence.
  Numerical tests exercise detection, noisy image registration, rotation,
  outliers, false matches, sigma clipping, coverage and lossless linear export.
  See [pipeline details and limitations](LIVE_STACKING.md).

## 1 November 2026: single-window workspace (Windows 0.6.2)

- Replaced independent native viewports with one normal resizable Windows
  container and a Dear ImGui dockspace. Camera operations and Android code are
  unchanged; this release distributes a new MSI only.
- UI tests exercise dock bounds and visible non-overlap at 640×480, 800×600,
  1280×720, 1920×1080 and 2560×1440 with 75%, 100%, 150% and 200% scaling.
  Tests cover automatic regrouping, the initially visible image tab, preserved
  custom tabs after restart/resize, reopening closed panels and recovering a
  panel dropped outside a dock target.
- Native OpenGL smoke checks validate one viewport/taskbar entry and six docked
  panels; rendered layouts were inspected at wide, medium and compact sizes.
  The existing desktop layout is retained; the new arrangement is saved to
  `workspace-layout.ini`.
- No shutter release or optical autofocus validation was required for this UI
  change. The earlier physical-camera results below retain their original scope.

## 1 November 2026: point autofocus and Android focus controls

- Windows 0.6.1 / Android 0.1.2 introduce native single-point Live AF, separate
  from manual point measurement and stellar autofocus. The 1100D integration
  test focused two distinct points and obtained camera-confirmed OK results
  without taking photos (12.93 seconds).
- The physical Pixel 8 Pro instrumentation passed in 18.78 seconds: actual
  image taps produced native camera confirmation in portrait and landscape;
  Near/Far and mode changes were disabled during AF; Stop cancelled a second
  operation; Measure and Off were both selectable. A numeric stellar-AF setting
  was changed through the dedicated sheet, confirmed in PC state, then restored.
  Screenshots of both orientations were inspected. Stellar AF itself was not run.
- The Android command bug was a dropped second command: switching to 1× set the
  busy flag, then the immediately following mode command was discarded. Mode
  changes now await command completion and a fresh frame before changing UI state.
- 233 automated CTest entries passed with the real CR2 fixture enabled; Android
  debug build, lint and JVM protocol tests also passed. Worker tests cover
  conflicting queued capture, lens, zoom, settings and AF commands; domain tests
  cover timeout, cancellation, disconnection and failed AF stop. A failed stop
  closes the camera session before allowing further use.
- Camera OK is the native contrast-AF result for that operation, not a guarantee
  of stellar optical accuracy, subject tracking or continued focus after physical
  movement. Tests used the connected indoor scene; no photos were taken.
- The extended final Pixel test passed in 55.58 seconds, including the focus
  checks above, centred zoom and a normal/Turbo throughput comparison. After an
  off-centre AF target, the normalized zoom centre was (0.50047, 0.50070) at 5×
  and (0.5, 0.5) at 10×; the small 5× difference is native coordinate rounding.
- Normal/Turbo measured 9.24/21.97 acquired FPS and 7.07/10.81 Android FPS with
  the same 1024×682 remote frame and normal peaking enabled. See
  [Live View performance](LIVE_VIEW_PERFORMANCE.md) for settings and limits.

## 31 October 2026: distribution artifacts

- Windows 0.6.0 is packaged as an x64 MSI with the Canon SDK runtime, app-local
  Visual C++ dependencies, licenses and documentation. The MSI is not
  Authenticode-signed.
- Android 0.1.1 release build and release lint passed. The APK has a verified
  APK Signature Scheme v2 signature using the project's RSA-3072 distribution
  key. This differs from the earlier debug key, so a debug installation must
  be removed once before installing the release APK.
- The physical tests below used the debug build of the same application code.
  The signed release variant was built and verified, not reinstalled over the
  user's paired test app. This does not extend the optical or Wi-Fi validation.
- SHA-256 sidecars accompany both downloadable installers.

## 31 October 2026: Android lifecycle, peaking and capture refinements

- 223 CTest entries passed, including two new worker regressions: EVF must be
  off before the shutter, remain off until download completion, and resume only
  when it was active before a successful capture. An initially stopped EVF is
  not enabled by an unrelated capture.
- Android 0.1.1 debug build, lint and four JVM protocol tests passed. The updated
  APK is installed on the physical Pixel 8 Pro. Two instrumentation tests passed
  in 67.8 seconds, including the complete physical 1100D workflow.
- The phone went to its launcher for 35 seconds, across WebSocket ping intervals,
  then returned with the same authenticated connection. Android reported the
  connected-device service in foreground state. Live preview resumed at 7.3 FPS.
- Portrait -> landscape -> portrait retained pairing and the real Canon preview.
  Screenshots were inspected: landscape has a separate preview and control rail;
  the mode tabs remain accessible. Normal and Stars peaking were selected through
  Compose callbacks and confirmed in PC state, retaining the previous image.
- Actual shutter and sequence UI actions produced three CR2 files on the PC;
  Live View automatically resumed after the single capture and final sequence
  download. All original settings were restored and EVF was left enabled.
- After successful authentication, the native PC pairing dialog was no longer
  present. Stellar autofocus and lens movement were deliberately not invoked.
- As in the preceding validation, networking used the USB-tethering LAN. Direct
  Wi-Fi and long-duration screen-off endurance were not validated in this run.

## 31 October 2026: Android LAN remote controller (0.6.0)

- Windows: 221 software CTest entries passed with the real CR2 fixture. Coverage
  includes actual WebSocket sockets, challenge-response, tampering/replay,
  server restart, binary JPEG bounds/crop, gallery path containment and nullable
  configuration map entries. Module boundary checks passed.
- Android: four JVM protocol tests, debug APK build and Android lint passed.
  The developer-signed APK was installed by ADB on the physical Pixel 8 Pro
  (Android 17). Two instrumentation tests exercise bounded bitmap decoding and
  the real camera workflow; no camera SDK behavior is simulated.
- The phone decoded a fresh screenshot of the PC QR using the same ZXing parser,
  rejected a deliberately wrong pairing secret, then authenticated with the
  valid one-time QR. The optical phone-camera scan itself needs a user aiming
  the phone at the PC; automated validation uses the actual QR pixels.
- ISO, aperture and shutter were written and checked against camera readback.
  Real 1x/5x/10x preview transitions, including the 1100D 10x crop, were verified.
  Binary Live View displayed approximately 6.5-7.5 FPS during the test runs.
- A real single exposure and two-frame sequence with a two-second interval
  produced CR2 files in a distinct `android-validation-*` PC session. Capture
  previews, PC gallery images, peaking, histograms and focus-point controls were
  exercised. Original camera settings/configuration were restored and Live
  View was left running. No stellar AF run or lens movement was commanded.
- The test used real IP sockets over the USB tethering LAN, without ADB port
  forwarding. Direct Wi-Fi was **not verified**: the PC Wi-Fi was disconnected,
  and Windows refused connection changes without location permission. No
  location settings, firewall policy or privilege elevation was changed.
- Native desktop smoke: seven non-overlapping rounded panels, safe content
  bounds, hidden host and exactly one taskbar entry at 1400 x 900.
- Transport is authenticated/signed plain WebSocket, not encrypted WSS. Use a
  trusted local network. This is not a claim of outdoor focus accuracy or
  validation of every possible camera mode, network or Android device.

## 31 October 2026: visible version and faint-star candidate gate (development)

- 212 software tests passed, including a faint compact source rejected by the
  former discovery threshold and accepted after accounting for smoothing noise
  gain. Raw photometric checks remain unchanged. Independent and correlated
  noise fields across 24 seeds remained rejected, along with existing hot-pixel,
  clipping and trailing cases.
- A native desktop smoke run confirmed the visible `AstroCanon v0.5.0` title,
  seven non-overlapping panels, unclipped content and one taskbar entry. The
  toolbar's stable ImGui ID remains unchanged across version updates.
- No real-sky camera frame was available for this correction. It does not
  establish faint-star autofocus performance outdoors or add capture-based AF.

## 31 October 2026: coarse recovery survey and small-drive measurements (development)

- 210 software CTest entries passed with the real CR2 preview fixture. New tests
  cover zero-star Near/Far acquisition, optical return after commands saturate
  at a travel limit, wrong returns, missing/frozen frames, geometry changes,
  AF/MF gating, cancellation and ambiguous movement. Small-stage tests cover
  temporal settling, an unresponsive lens, repeatable four-command mechanical
  play in a domain test double, warning policy, configuration and translations.
  The final suite also checks that a transient source cannot be counted as
  multiple persistent stars. Domain tests do not emulate Canon SDK or prove optics.
- Native MCP on the connected EOS 1100D reproduced the original homing failure:
  reverse response was below the measured noise threshold. With the new recovery
  path, the same camera completed a 64-command coarse survey, independently
  verified the return, and reached all three drives without reconnecting.
- In the first physical run (monitor star image, ISO 200, f/5.6, Tv 1/5 s),
  Large and Medium returned measured HFD values of 3.963 and 2.124 px. Small used
  11 frames per source, widened spacing to 16 small commands, and used 22 frames
  on final verification. Its stationary paired noise was about 0.108 px.
  Reversal response could not be calibrated, so the configured compensation
  remained active. The final return HFD was about 2.471 px after 380 total
  commands. It did not pass the comparison against the completed stages and
  correctly remained an error, not a warning or verified optical success.
- A second native-MCP run temporarily used ISO 800 with the same f/5.6 and
  Tv 1/5 s. It again recovered through the survey, verified Large and Medium
  (4.046 and 2.226 px), and ended after 385 commands with a verified return of
  2.459 px and a **precision-limited warning**. Small remained below noise and
  its final stage result stayed unset. This is qualified recovery, not a precise
  Small success. ISO 200 was restored afterward; no photographs were taken.
- Italian and English diagnostic messages were checked through native MCP.
  The new sidebar fields were inspected in a real panel image. Settings and
  camera ownership remained in the same native UI/core/MCP architecture.
- These measurements validate acquisition recovery and rejection of an inferior
  return. They do **not** establish reliable fine-focus convergence or real-sky
  accuracy. Full diagnostic snapshots are local validation artifacts, not
  synthetic success evidence.

## 31 October 2026: Live View throughput and bilingual logs (development)

- 196 software CTest entries passed with the real CR2 preview fixture enabled.
  This includes timer cancellation/deadlines, per-frame AF analysis, unchanged
  histogram bins over all RGB colors, and bilingual logging under concurrent use.
- On physical 1100D 1056x704 Live View, the optimized build delivered 26.52
  displayed FPS with normal peaking and 24.58 with stellar peaking. Histograms
  and reticles remained enabled; exposure was restored after a diagnostic check.
  See [measurement conditions and method](LIVE_VIEW_PERFORMANCE.md).
- This performance/logging work does not add a physical autofocus-success claim.
  The previously recorded optical-focus limitations remain applicable.
- Native MCP checks confirmed English/Italian messages on the same retained
  connection/recovery events, localized substring filtering, stable severity
  IDs and localized severity labels. Actual `astro.log` writes followed language
  changes without a worker restart. The physical 1100D reconnected during this
  check; no exposure or lens movement was requested. Both log-panel languages
  were inspected through native panel captures.
- The full 513-check HTTP/stdio suite below belongs to the camera-module
  validation; it is not presented as a new run for this development revision.

## 31 October 2026: dedicated Canon EOS 1100D module (0.5.0)

- All 179 software tests passed after extracting the 1100D model policy. Tests
  cover model matching/rejection, backwards-compatible configuration, protocol
  values for Bulb/transfer counts and AF drive, plus the preserved 10x pixel crop.
- Five hardware tests passed with the physical Canon EOS 1100D. Automatic,
  explicit 1100D and automatic selection again worked in one process; session
  reopening, property reads, unchanged ISO write/read flow, PC-only destination,
  ten Live View frames and zoom 1x -> 5x -> 10x -> 1x were exercised. The 5x JPEG
  was 848x568 and the 10x displayed crop was 424x284.
- 513 native MCP checks passed across separate HTTP and stdio application runs,
  including auto/manual model selection on the physical camera, blocking module
  changes while connected, clearing identity on disconnect, standby protection
  and real Live View zoom frames. Connection panel captures were inspected.
- The lens reported MF during this validation. No shutter or lens movements
  were issued; this refactor does not add a new optical autofocus accuracy claim.
  No model other than the EOS 1100D has been implemented or validated.

## 31 October 2026: unclipped native windows and qualified AF warning (0.4.3)

- All 170 software CTest entries passed with the real CR2 preview fixture;
  181 native MCP integration checks passed with HTTP and stdio run separately.
- Native corner regions are constrained to each window's actual content padding,
  including panels, popup menus and submenus. Region cache invalidation includes
  the corner diameter. Seven-panel native smoke checks passed at 75%, 100%,
  150% and 200% simulated DPI, checking content corners, non-overlapping layout,
  work-area containment and one taskbar entry. The Settings/Language menus were
  inspected interactively at 100%; panel captures were inspected at 100% and 200%.
- A noise-limited small-drive stage now reports a terminal warning only after
  the large and medium stages succeeded and a fresh measurement confirms the
  best return against the stored stages. Failed returns and connection loss
  remain errors. Regression tests exercise both outcomes. No new optical AF run
  was performed for this classification/UI change; final fine precision remains
  explicitly unconfirmed in the warning result.

## 31 October 2026: self-contained installer and stable AF controls (0.4.2)

- All 169 software CTest entries passed with the real CR2 fixture enabled.
- Field HFD and candidate-count rows remain present with zero detections;
  completed-stage results render below Start/Stop, keeping those buttons in
  place as detections and stage results change. The zero-star native UI was
  inspected from the extracted installer application.
- The MSI includes both Canon runtime DLLs and app-local x64 Visual C++ DLLs.
  Packaging validates required files, architecture and matching versions, and
  includes a dependency manifest with versions/hashes and original notices.
- Administrative MSI extraction matched all 35 staged files by hash. The
  extracted executable passed the native seven-panel / one-taskbar-entry smoke
  test with a system-only PATH. Process module inspection confirmed EDSDK and
  four required Visual C++ DLLs loaded from the extracted application directory.
  This is package validation on the development PC, not a clean Windows VM test.
- The optical autofocus limitations from 0.4.1 remain; this packaging/UI update
  does not claim improved or fully verified final small-drive precision.

## 31 October 2026: travel calibration, return recovery and sleep protection (0.4.1)

- The Release build passed **169 software CTest entries**, including the real
  CR2 preview fixture, and **180 real-process MCP checks** with HTTP/stdio tested
  separately. Core/UI/SDK boundary validation passed. New domain tests cover
  repeated near resets from different starting positions, two-end calibration,
  stationary motors, blank fields, cumulative slow motion, stale frames during
  queued moves, cancellation/budgets and configuration round trips/bounds.
- The physical EOS 1100D remained connected during the tests, with its lens in
  AF and a photograph of stars displayed on a monitor. Later trials used M,
  ISO 100, f/5.6 and 0.5 s. Only Live View and relative lens commands were used;
  no still exposure was commanded. This is not a real-sky accuracy test.
- Every production AF start now calibrates near and far optical travel limits
  and returns near before searching for stars. Two 1x trials completed this
  calibration: 59 commands / estimated eight large-command travel, then 67
  commands / estimated nine large-command travel on the final build. The
  difference reinforces that these are optical estimates, not encoder units.
  The final trial then verified large HFD 4.4897 px and medium HFD 2.2240 px.
- Earlier apparent endpoint plateaus did not reproduce and were rejected.
  The final detector compares cumulative motion, checks a reverse probe and
  verifies return; it permits one bounded full re-verification. A 5x trial
  stopped after one large move because defocus removed the stable image signal.
  It did **not** report a calibrated endpoint or continue blindly.
- Small-drive response remains inconsistent in this setup. At 5x, ten Far1
  commands produced little change, while one Far2 command enlarged a tracked
  star from about 5.5 to 8.9 px HFD. Twenty Near1 commands reduced it to about
  6.8 px; eight more Near1 commands left it near 6.7–6.8 px. Accepted SDK calls
  therefore cannot be treated as measured absolute displacement. Full verified
  small-stage focus and real-sky optical accuracy remain unconfirmed.
- The complete final-build run ended after 329 total commands (67 calibration),
  on pass three, with `Small-drive response below measured noise; precision
  unconfirmed`. The final fine curve had R2 0.449 and leave-one-out uncertainty
  4.85 small commands, failing the configured 0.85 / one-command criteria.
  The measured best return was rechecked (`restored=true`); final precision was
  not marked verified. Camera and Live View remained connected afterward.
- Connected-session Windows system/display power requests succeeded and were
  visible through MCP as `sleep_inhibited=true`, including connected idle.
  Native lifecycle and worker disconnect/reconnect/loss tests passed. Canon's
  existing ten-second shutdown-timer renewal remains active. Forced sleep,
  lid-close and critical-battery behavior are not overridden; a full Windows
  inactivity/suspension test has not been performed.
- The physical camera was also explicitly disconnected and reconnected in the
  same final-build process: `sleep_inhibited` changed true -> false -> true;
  disconnection invalidated both focus limits and reconnection kept Live View
  stopped. The 0.4.1 per-user MSI installed successfully (exit 0); hashes matched
  build, portable and installed executables, and three existing personal files
  (settings, layout and output) retained their hashes.
- Private frame/state evidence is under `out/validation/oct31-*`; transport
  checks are under `out/validation/mcp-oct31`. Neither private camera images nor
  credentials are committed.

## 24 October 2026: real-camera test through native HTTP MCP (0.4.0)

- The installed application connected to the physical EOS 1100D through its
  native MCP endpoint. The lens reported AF-ready; exposure was M, ISO 1600,
  f/5.6 and 0.5 s. No still exposure or exposure-setting change was commanded.
- Live View returned 1056 x 704 frames and exact rendered panel screenshots.
  UI, core and MCP reported three distinct thread IDs in the same process;
  UI/core counters and frame revisions continued advancing after the AF failure.
- The monitor scene initially yielded zero valid stars and 18 broad candidates.
  Automatic acquisition found persistent stars after one large-drive command,
  with baseline HFD 4.9035 px. The large-stage scan then lost matching stars
  during its return and failed after nine total motor commands with
  `Not enough stable matching stars at return or baseline`.
- No stage was verified; medium and small stages were never reached. The return
  was not verified and the lens remained at the last commanded position. This
  reproduces an autofocus failure, not a successful optical calibration.
- The camera remained connected with Live View running after the failure.
  Local evidence is under `out/validation/mcp-hardware-*` and
  `out/validation/mcp-live-before.png` / `mcp-live-after.png` (ignored private
  artifacts). The named Codex MCP tool was unavailable in this chat, so requests
  used the same native HTTP endpoint directly, without a replacement server.

## 24 October 2026: native MCP and thread ownership (0.4.0)

- MCP is compiled into the application in C++; HTTP and stdio are mutually
  exclusive launch modes. There is one configured Codex server, `astrophoto`.
- The UI, core and MCP have distinct thread IDs in one real executable. Photo
  decoding now runs on the core worker; HTTP requests use the MCP thread itself.
- Protocol/queue tests cover concurrent producers, independent replies, bounded
  capacity, expired requests, error acknowledgements and clean shutdown.
- The Release build passed **150 software CTest entries** and **179 native MCP
  integration checks**. The real CR2 fixture was enabled. These counts include
  interruptible core shutdown, stdio EOF shutdown and conflicting-mode rejection.
- Real-process integration verifies both transports separately, UI/configuration
  edits, native panel PNGs, actual disconnected-camera errors, HTTP auth/Origin
  checks, concurrent HTTP requests on one MCP thread and the absence of an HTTP
  listener in stdio mode. Tests restore settings and close each process normally.
- These are control-path tests without connecting the camera. They do not add
  evidence of successful optical autofocus; that remains the next hardware task.

## 24 October 2026: Hocus Focus adaptation (0.3.1)

- All **134 software CTest entries passed**, including the enabled real-CR2
  preview fixture. New coverage includes mandatory large/medium/small stages,
  verified returns, same-star comparisons with temporary dropouts, bounded
  breathing registration, source-sized photometric apertures, targeted known-star
  measurements, the native Hocus Focus numerical routines and robust background
  subtraction. Regression tests reject a flat fine response and require an
  improvement above temporal noise even when a mathematical fit has excellent R².
- On an identical previously saved 1100D frame, fixing the fixed-aperture
  rejection changed zero detections into 110 usable stars. This is a detector
  regression observation, not an optical autofocus result.
- Physical trials on the user's monitor scene still exposed non-repeatable
  returns after relative Canon commands. The first Hocus-integrated trial stopped
  after 73.9 seconds without a stable matching return. Separating scouting from
  the continuous farward precision sweep then acquired stars after 18 large
  commands, verified large (5.04248 px) and medium (5.33010 px), and stopped after
  245.5 seconds because the small-drive data did not support a reliable minimum.
  No successful final focus was reported. Neither trial triggered the shutter
  or changed exposure settings.
- The fine sampler now increases spacing fourfold when its response is below
  three times temporal scatter, while continuing to issue small Canon commands.
  Its final leave-one-out uncertainty is bounded in individual small commands,
  even when the sample spacing is larger. A paired-star, noise-resolved bracket
  is also mandatory; covariance/R² alone cannot confirm optical response.
- The subsequent adaptive-spacing monitor trial ran for 282.6 seconds and
  stopped at 189 commands while preserving the movement limit. It verified
  large at 3.09987 px and medium at 2.62438 px, but the final curve had weighted
  R² 0.307512 and leave-one-out uncertainty 16.1055 small commands. Final focus
  was not confirmed. The final software build additionally tightens acceptance
  with the paired-star noise-resolved bracket described above; that extra
  rejection gate was software-tested after this physical run's binary was built.
  **There is still no passing end-to-end physical stellar-autofocus test.**
- The full upstream reference repository is available locally, pinned and
  verified. [Adaptation notes](HOCUS_FOCUS.md) specify the reviewed and ported
  components, unported features and MPL-2.0 source distribution.

These monitor trials do not establish real-sky accuracy, calibrated backlash,
or an absolute/global focus optimum for arbitrary lenses and focal lengths.

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

### Automatic star acquisition and adaptive focus (0.3.0)

- All **107 software CTest entries passed**, including real CR2 preview decoding. New regressions cover zero-star acquisition in both directions, empty fields, stale frames, coarse-scale acquisition hints, flat/off-center curves, small commands without optical response, insufficient coarse samples, temporal outliers, cancellation and independent return verification. Synthetic motor doubles test the controller only; they do not replace Canon hardware.
- On the real EOS 1100D aimed at the user's new monitor star image, an initially fully defocused frame contained **zero native and zero coarse detections**. The acquisition pass found a persistent cohort after **18 large Canon search commands** and automatically started precision sampling (initial cohort HFD about **3.17 px**). No still exposure or exposure-setting change was commanded.
- The first precision run used repeated small commands, produced shallow curves and eventually lost its matching cohort. The next implementation increased the actual Canon command size after a flat scan. Medium commands then measured HFD values from approximately **3.14 to 4.43 px**, but several positions could not retain the fixed cohort. That trial stopped when the camera no longer reported M/fixed-ISO/non-Bulb settings. The following session opened but delivered no Live View frame in 15 seconds.
- The final controller additionally reduces command size and re-anchors when a coarse scan has too few measured points, and waits for a clean temporal window instead of failing on one outlier. A successful physical return on this scene remains unverified; no failed/flat curve was reported as successful focus.
- The English native UI smoke test at 150% on a 1920x1080 work area passed with seven non-overlapping rounded panels and one taskbar entry. The focus sidebar was visually inspected. Private monitor photographs and diagnostic captures remain outside Git.
- After the operator verified settings and power-cycled the camera, the final retry could not enumerate a Canon device; Windows also showed no present Canon USB device. Therefore the final optical test is outstanding, not passed. The 0.3.0 MSI installed with exit code 0; hashes confirmed that the three captures and user settings were unchanged. Its installed executable matches the portable release and passed the native seven-panel/one-taskbar-entry smoke test.
- These monitor trials exercise acquisition and motor-control behavior. They do not measure real-sky accuracy, lens repeatability over multiple nights, or performance relative to calibrated focusers.

The default search permits a 16-command radius in each direction, starting with Canon step 3. Coarse-scale hints may reduce the step and reset the search origin once. All acquisition and precision moves share a 200-command/600-second budget; an empty field cannot cause an infinite sweep. Set `ASTRO_HARDWARE_STAR_FOCUS=run` to opt into physical motor testing and optionally set `ASTRO_FOCUS_DIAGNOSTICS_DIR` to a local output folder for per-movement JPEG diagnostics. These diagnostic images are not the settled measurement batches and are not proof of focus.

### Connection stability and recovery (0.2.1)

- The SDK wrapper previously ignored `WillSoonShutDown`, and the worker disconnected on any polling/analysis exception. The wrapper now renews Canon's auto-shutdown timer every configured interval (10 seconds by default) and handles shutdown warnings outside callbacks. Busy/not-ready polling errors preserve the session; image-analysis failures do not close USB. A transport loss stops capture/focus and triggers bounded, cancellable reconnect attempts without restarting Live View or exposures.
- Empty enumeration, stale session or invalid transport handles during connection trigger at most one SDK restart per attempt, after all camera/list references have been released. USB-in-use errors do not trigger an SDK reset. This does not alter camera power settings or Windows USB policies.
- All **90 software CTest entries passed**, including temporary pump/frame failures, analysis failure isolation, transport recovery without resuming capture/Live View, explicit cancellation and bounded retry exhaustion followed by manual reconnect. Tests use domain interface doubles, never a simulated EDSDK.
- A real EOS 1100D remained connected for **150 seconds idle plus 30 seconds of Live View**, then disconnected/reconnected in the same process (181 seconds total). No still exposure or lens movement was commanded. The subsequent final code only broadened bounded connection recovery and throttled repeated diagnostics; the timer implementation was unchanged.
- The user then powered the 1100D off and back on during the final-build `Hardware.PowerCycleReconnectWithoutRestart` test. The stale handle was released despite the expected powered-off cleanup error `0x61`; the same SDK process rediscovered the camera, read its properties and opened another clean session. The test passed in 38.6 seconds.
- The regular final hardware suite also passed, including repeated sessions, lens state and Live View/zoom. The MSI upgraded the user's installed 0.1.1 to 0.2.1 with exit code 0. SHA-256 checks confirmed that the three existing captures and personal settings file were unchanged. The installed executable matches the portable release and passed native UI smoke validation (seven panels, one taskbar entry).
- The supplied monitor image contains visibly elongated star images; it is useful for a display-scene trial but cannot establish real-sky optical focus. It is unrelated to USB connection management and was not added to the repository.
- Long-duration overnight operation and arbitrary cable/power faults remain outside this short validation run.

Run the three-minute connection test with `ASTRO_HARDWARE_CONNECTION_SOAK=1` and `astro_hardware_tests.exe --gtest_filter=Hardware.IdleKeepAliveAndLiveViewSoak`; close the app first. The operator-assisted power-cycle test uses `ASTRO_HARDWARE_POWER_CYCLE=1` and `--gtest_filter=Hardware.PowerCycleReconnectWithoutRestart`.

### Stellar autofocus and right-hand focus sidebar (0.2.0)

- Final Release compilation, all **84 software CTest entries**, module-boundary checks and the English native smoke test at 150% on a 1920×1080 work area passed. The smoke test found seven non-overlapping rounded panels and one taskbar entry; the right-hand sidebar screenshot was visually inspected.
- The 0.2.0 MSI passed WiX validation with the existing font-language ICE60 warning. Administrative extraction returned exit code 0; its executable hash matched the portable release and its native smoke test passed with seven panels and one taskbar entry. The 0.2.0 installer was not installed over the user's existing installation during this check.
- The final regular hardware rerun failed its four connection-dependent cases because the SDK no longer detected the camera. The two explicitly opt-in tests were skipped. This does not supersede the earlier monitor observations below or establish a successful physical autofocus run.
- HFD tests compare synthetic Gaussian point sources with the analytic half-flux diameter, increase blur at constant flux, vary subpixel position and brightness, and reject clipping, hot pixels, trails and noise. The domain controller tests cover a full sweep/verified return, flat or unbracketed curves, failed return verification, missing/cohort-changing frames, transient detections, MF/disconnection, cancellation and bounded/ambiguous motor commands. These domain doubles are not linked into the app or used instead of the physical SDK.
- On the physical EOS 1100D aimed at the user's star-field monitor, 1056×704 Live View frames produced approximately 7–17 usable stellar detections and many clipped sources. Analysis took roughly 25–32 ms per frame. Monitor pixels/moiré, flicker, viewing angle and image clipping limit what this trial establishes.
- Two initial runs stopped during baseline acquisition without moving the lens because a stable matching cohort could not be established. The first-frame selection was improved to require persistent detections across several frames.
- A diagnostic run temporarily selected the camera's available ISO 400 value, then restored the previous ISO. It acquired a baseline HFD of 2.867 px and issued eight smallest-step commands for the initial approach. At the first scan position, stable matching stars were insufficient; it stopped without claiming success or issuing a return-to-best move. The lens was left at that commanded position, as specified by the failure policy.
- A successful autofocus return on this physical setup, real sky operation, measured backlash calibration and optical accuracy at different focal lengths remain unverified. The feature reports these failures rather than weakening acceptance criteria to force an OK result.
- The UI groups motor buttons/step selection, point focus, zoom, peaking, reticle/histograms, star AF parameters/curve and exposure advice beside the image. Motor controls remain visible with point focus disabled; AF-lens/Live View/capture gating remains mandatory. Advice never changes exposure automatically.

For the opt-in monitor test, set `ASTRO_HARDWARE_STAR_FOCUS=analyze` for measurement only or `run` for the bounded motor scan, then run `astro_hardware_tests.exe --gtest_filter=Hardware.StellarFocusOnRealLiveView`. Optional `ASTRO_FOCUS_TEST_ISO` temporarily selects an available ISO by its displayed label and restores it. No still exposure is taken.

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

Camera-only saving, RAW-to-TIFF/JPEG export, individual PictureStyle parameters,
free manual EVF panning, Bulb telemetry and EXIF-based automated capture validation.
Native AF target positioning and automatic zoom centring are implemented.

## Running the existing tests

```powershell
./scripts/build.ps1 -WithEDSDK -HardwareTests
$env:ASTRO_HARDWARE_TESTS = '1'
$env:ASTRO_HARDWARE_LIVEVIEW = '1'
ctest --test-dir out/build -C Release -L hardware --output-on-failure
```

Close the desktop app first to release its session. Enable Live View testing only when the physical camera is available. The current integration suite does not trigger still exposures.
