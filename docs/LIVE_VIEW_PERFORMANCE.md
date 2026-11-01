# Live View performance validation

## 1 November 2026: Turbo Live Stream and Android

Windows 0.6.1 and Android 0.1.2 add an optional **Turbo Live Stream** switch.
It uses a 1 ms camera request interval, removes the hidden host window's VSync
wait and allows up to 60 remote preview requests per second. Only one Android
image request may be outstanding. Actual frame delivery remains limited by
the camera, image processing, encoding and network round-trip time.

The physical EOS 1100D and Pixel 8 Pro were tested through the authenticated
WebSocket connection with normal peaking enabled, 1× zoom, ISO 200, f/11 and
0.5 s Tv. Each mode had a three-second warm-up followed by twelve seconds of
sampling. The final run measured:

| Mode | Camera acquisition | Android preview | Remote frame |
| --- | ---: | ---: | --- |
| Normal (existing 100 ms acquisition setting) | 9.24 FPS | 7.07 FPS | 1024 × 682 |
| Turbo | 21.97 FPS | 10.81 FPS | 1024 × 682 |

Turbo preserves the normal remote JPEG quality setting (0.78), resolution cap
(1024 pixels), peaking and histogram processing. It does not make JPEG lossless
or guarantee 60 delivered FPS. Remote Live View uses the already processed CPU
pixels instead of a GPU readback; unchanged frames reuse the encoded JPEG.
Existing acquisition timing is preserved when Turbo is switched off.

The final 55.58-second Android hardware test also verified camera-confirmed
point AF in both orientations, cancellation, conflicting-control locks and
centred 5×/10× zoom after selecting an off-centre AF target. No still photo or
stellar autofocus run was triggered. These indoor measurements are not directly
comparable with the different scene/settings in the October measurements below.

## 31 October 2026: desktop acquisition optimization

Validated on 2026-10-31 with the physical Canon EOS 1100D through the native
AstroPhoto MCP connector. The camera faced a monitor displaying a star field;
these are Live View throughput measurements, not sky autofocus validation.

## Measured results

The first optimization pass, at ISO 1600, f/5.6, 1/15 s and 1x, produced
17.96 acquired FPS (15.15 seconds, peaking off) and 17.39 FPS (33.64 seconds,
stellar peaking). The request interval was 33 ms.

The maximum-throughput pass used ISO 200, f/5.6, 1/15 s, 1x and the original
1056x704 JPEG frames. Histogram and crosshair remained enabled. A brief 1/60 s
diagnostic was interrupted when the app closed; it is not used as evidence of
an exposure-related gain. The original 1/15 s exposure was restored. No lens
movement or shutter capture was commanded.

- With a 1 ms request interval but the old Windows wait, acquisition reached
  21.40 FPS over 25.51 seconds.
- Replacing that wait with a high-resolution timer produced 27.00 acquired FPS
  and 26.30 displayed FPS over 22.67 seconds, with peaking off.
- Final build, normal peaking: 26.71 acquired FPS and 26.52 displayed FPS over
  25.45 seconds. This includes live histogram calculation and rendering.
- Final build, peaking off: 26.65 acquired FPS and 26.21 displayed FPS over
  31.55 seconds.
- Final build, stellar peaking: 26.75 acquired FPS and 24.58 displayed FPS over
  25.80 seconds. Average stellar-overlay processing cost was 22.87 ms/frame.

Rates use deltas of native `get_state` counters divided by wall time. The UI
counter counts unique displayed frames; skipped camera revisions and repeated
UI redraws do not inflate it. Final sample windows exclude screenshot calls,
compilation and tests. These are observed rates on this setup, not a guaranteed
camera ceiling or a promise of the same FPS during autofocus, at other zooms,
in different scenes, or on other computers.

## What changed

- Default acquisition requests now have a 1 ms minimum interval. Previously
  the 100 ms default imposed a 10-FPS ceiling before processing overhead.
- A local, cancellable Windows waitable timer replaces the coarse worker wait.
  On this machine a requested 1 ms condition-variable wait took 15.99 ms median;
  the new timer took 1.51 ms. No busy-wait or system-wide timer-resolution change
  is used. Older Windows versions fall back to a regular cancellable timer.
- Idle stellar statistics update every 250 ms; autofocus still analyzes every
  acquired frame with full photometry. Lens status polls independently at 500 ms.
- Worker waits follow acquisition deadlines. The UI idle budget includes image
  processing and rendering, instead of adding a fixed delay afterwards.
- Uncropped decoded images transfer ownership; disabled peaking avoids a full
  image copy. Same-size texture updates reuse GPU storage.
- Stellar peaking uses native star photometry without the AF travel signature
  or multiscale acquisition search. The HFD integration skips only pixels whose
  entire subdivision square lies outside the aperture; contributing samples,
  summation order, `hypot`, and 4x4 subdivision remain unchanged.
- Histogram rounding exploits the bounded, nonnegative luminance range without
  a general-purpose rounding-library call. Every pixel still contributes.
- Native MCP exposes cumulative acquisition and UI-stage timing for diagnosis.

The Windows high-resolution flag is supported starting with Windows 10 1803:
[Microsoft CreateWaitableTimerExW documentation](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-createwaitabletimerexw).

## Validation

All 189 software tests passed, including the real CR2 fixture enabled with
`ASTRO_PREVIEW_CR2`. Coverage includes timer cancellation/reuse, deadlines,
idle-analysis pacing, per-frame AF analysis, FPS stalls/revision resets,
unchanged peaking photometry, all 16,777,216 RGB colors for histogram bins, and
zoom crop behavior. The native desktop smoke test passed with seven bounded,
nonoverlapping panels and one taskbar entry. Module boundaries also passed.

`astro_live_benchmark recorded-live-frame.jpg` measures individual CPU stages;
`astro_live_benchmark --timers` compares waits without opening the camera.
On the recorded 1056x704 frame, final median times were 27.15 ms for full stellar
analysis, 27.27 ms for stellar peaking and 2.43 ms for the histogram. Before the
last optimization these were 31.19, 30.61 and 3.74 ms respectively (the histogram
baseline precedes the rounding optimization). The recorded frame's 111 detected
stars had identical printed positions, HFD, flux and SNR at 17-digit precision
before and after the aperture exclusion optimization.

File-decoding benchmarks include file access and COM initialization; they are
not the application's in-memory decode cost or camera FPS. Scene complexity
affects stellar-analysis time.

Existing user files preserve explicit timing choices. The local `out/Release`
configuration uses 1/250/500 ms for acquisition/idle analysis/lens polling. The
pre-optimization executable and configuration are retained under
`out/preserved/live-fps-before`. Final validation artifacts are under
`out/validation/live-max-*`.
