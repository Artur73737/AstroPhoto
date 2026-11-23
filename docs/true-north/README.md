# True North compass and ZWO ASI — Windows 0.10.1

All acquisition, image processing, astrometry and UI code is C++20. ASTAP source extraction and the C++ Gaia polar matcher run
on demand; packaged examples contain image pixels, not cached solutions.

## Physical input contract — section 3.6

**A solved photograph alone cannot establish a physical compass orientation.**
The user must verify the camera and sensor are level, rigid, upright landscape,
unmirrored, with known zero roll within 0.1 degree. Do not move the camera relative
to the level after levelling. Supply the actual WGS84 site and exposure-midpoint
UTC and realistic clock/level uncertainties. Check the explicit declarations in
the UI. Software cannot measure the truth of these declarations.

Geographic north at the horizon, the celestial pole of date, Polaris and the
image centre are different directions. The white centre reference is not a
measured principal point. The filled purple point marks the calculated celestial
axis. Off-frame directions are labelled OUTSIDE FRAME and never presented as
in-frame pole locations. The red direction is the geographic north horizon.

## Shared acquisition and ZWO controls

Select **ZWO ASI** in Connection, or automatic discovery (ASI is preferred if
one is enumerated; explicitly select the Canon module when both are attached).
Only one core camera owner exists. The regular Image modes **Live View**,
**Current capture** and **Live stacking** use that same adapter. True North does
not open a competing USB session.

ZWO controls use SDK-reported names, IDs, writable/auto flags, ranges and readback.
They are gain and exposure in microseconds, not fabricated ISO/aperture settings.
Enter a numeric value and press Enter to apply it. Single capture pauses video,
executes the native timed exposure, downloads RAW16 FITS, then the core restores
video if it was active. Stop/capture/focus/stack interlocks remain centralized.
ASI has no integrated lens focus motor; Canon lens AF commands are unavailable.
Images are PC-only. The current acquisition geometry is full active sensor,
bin 1, RAW16; arbitrary ROI/bin acquisition is not exposed in the shared adapter.

FITS records sensor name, gain (including valid zero), exposure, binning, Bayer
pattern or MONO, and host-estimated DATE-AVG in UTC. Clock uncertainty must include
PC synchronization and SDK command latency. This is not hardware timestamp
certification. Stack inputs accept CR2 and RAW16 FITS; mono data and RGB Bayer
patterns are handled separately. ASI colour development uses neutral unit gains
and an identity colour matrix, not an invented calibrated colour profile.
Physical ASI acquisition, orientation and colour accuracy remain experimental
until validated on an attached camera.

## File solve and live process status

Open **True North compass** within Image. Set image, ASTAP executable and catalog
paths. **Plate solve only** is the safe default: it projects the celestial pole
for the declared epoch and measures width/height FOV from the newly solved WCS,
but withholds geographic bearing. Disable it only with actual observer metadata
and all section-3.6 confirmations. **Capture and solve ZWO frame** submits one
fresh exposure through the same core; it never reuses a buffered JPEG as a
scientific exposure.

The UI displays queued/preflight, ASTAP running, WCS validation and elapsed time,
with cancellation and a bounded timeout. These are real process phases, not a
fabricated completion percentage. Competing ASTAP requests within AstroPhoto
are serialized. Failure clears prior coordinates. No solution means no pole
marker or invented compass result.

The three **User image** examples are the unmodified planetarium screenshots
provided in demo_photo. Their indices are not degree labels. They are differently
cropped rasters with grids, labels and artificial star rendering. They must not
be described as three independent camera exposures, assumed 1/2/3-degree fields,
or a physical compass validation. The C++ polar recovery now solves all three supplied originals using fresh ASTAP
centroids and Gaia catalogue correspondences; see the measured validation table.
The originals remain untouched. Bundled manifest includes original SHA-256 hashes.

## Astronomy

ERFA 2.0.1 handles UTC/TAI/TT, IAU 2006/2000A precession/nutation and TAN geometry.
The physical celestial axis is transformed using the transpose of the celestial-
to-intermediate rotation matrix; annual stellar aberration is not applied to an
axis. FITS WCS uses one-based pixel centres and an upward y axis; the display flips
y for projection. The horizon can be outside the TAN forward hemisphere, in which
case no finite pixel is reported. Near-zenith azimuth is ill-conditioned.

## Modules and checks

- src/cameras/zwo/sdk: the only module including ASICamera2.h.
- src/cameras/zwo/adapter: ICamera implementation and bounded JPEG display stream.
- src/infrastructure/camera_worker: backend selection, sole SDK owner, safety gates.
- src/imaging/fits: bounded primary-image decoding and atomic RAW16 output.
- src/features/true_north/astronomy: SDK/UI-independent calculations.
- src/features/true_north/workspace: ImGui controls and asynchronous solve workflow.
- src/features/true_north/tools: CLI and DSS numerical fixture utilities.

Each feature separates include/astro/... headers from src implementations.
Run scripts/check-source-layout.ps1 and scripts/check-boundaries.ps1. CMake also
compiles every public header independently. See VALIDATION.md for measured results.

The MSI includes ASI SDK runtime, signed ZWO USB driver staging, Canon EDSDK runtime
(Windows native PTP handles Canon USB), ASTAP and complete offline catalogs, and
app-local Microsoft runtime dependencies. No camera hardware is emulated.

## Polar recovery

The recovery is limited to the northern G05 polar cap (declination above +85°)
and image heights approximately 0.1–12 degrees. It reads the packaged catalogue,
builds scale-invariant canonical triangles and tests both parities. A solution
requires at least twelve unique matched sources, broad spatial coverage, RMS at
most 0.8 pixel and a conservative chance-match test over the bounded hypothesis
budget. It does not identify the pole from drawn grid lines or a Polaris label.
Only original source pixels are submitted for centroid extraction. The general
ASTAP solver handles fields that do not meet polar recovery requirements.

References: [ASTAP](https://www.hnsky.org/astap.htm), its published G05 catalogue
format, and [Gaia DR3](https://www.cosmos.esa.int/web/gaia/dr3). The catalogue epoch
and image epoch are not silently treated as a physical observation timestamp.
