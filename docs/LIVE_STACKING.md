# RAW live stacking

Live stacking is an **Image viewer mode**, not a separate window. Select
**Live stacking** to the left of **Current capture** and **Live View**. The same
preview area displays the accumulated image. On a wide workspace, stack controls
sit on the left; on narrow windows they move below the preview. The lower-left
overlay shows accepted frames, exposure count and elapsed/integrated time. The
histogram and Low/Mid/High controls are grouped with the stack settings, outside
the image, on Windows and Android. They affect display only.

## Acquisition

1. Connect the camera, choose RAW or RAW+JPEG, M exposure and fixed ISO in Camera.
2. Set aperture and shutter there. Stacking reads the actual camera properties;
   it does not silently change ISO, aperture, shutter or focus. In Bulb, duration
   comes from the single-capture duration setting.
3. Set the output root and session name in Files and session.
4. Choose a preset: **50, 75, 100, 200, 300, 500, 700 or 1000 exposures**, a custom
   count, or **Until stopped** (`frames = 0`). Counts limit shutter releases,
   including frames subsequently rejected by alignment.
5. Start. The app closes Live View, exposes, completes download, processes that
   RAW and waits the configured interval before allowing the next shutter release.

The interval defaults to 3 seconds and cannot be below 2 seconds. It starts after
processing, so slow downloads/processing lengthen the interval. There is one
outstanding exposure and one RAW processing job, with bounded queues. Ambiguous
shutter errors are never retried. Configuration changes, another capture,
sequences, lens movement and autofocus are blocked by the camera worker during
stacking, including when a client bypasses disabled UI controls.

For exposures longer than 0.25 seconds, the progress bar shows elapsed/requested
exposure time. This is host timing; a full bar does not mean download has finished.

**Stop is graceful:** no more exposures start, but the current exposure, transfer
and integration finish before the partial stack is saved. Live View resumes if
it was active before a successful run. Loss of USB, disk errors or changed camera
properties stop the run; they never automatically resume acquisition. Original
downloaded files remain on disk. Existing camera/card destination preferences
are honored. Spacing and serialization prevent conflicting commands; they cannot
eliminate normal mechanical shutter wear or compensate for failing hardware.

## RAW processing and alignment

- LibRaw unpacks the full RAW sensor image. The first RAW's as-shot white balance
  (daylight metadata fallback when unavailable) is frozen for the session and
  applied **before demosaic and the camera colour matrix**. Output is 16-bit
  **linear sRGB**, with no gamma, automatic brightness or scene white-point
  scaling. Embedded JPEGs are not accumulated. See the
  [LibRaw processing and colour metadata documentation](https://www.libraw.org/docs/API-datastruct.html).
- **Correct isolated hot pixels** operates on the original Bayer mosaic before
  demosaicing. It replaces extreme isolated high samples with the median of eight
  same-colour neighbours, preserving pixels with adjacent optical support. It is
  enabled by default and can be disabled on PC/Android. Counts are recorded for
  every frame. It is conservative cosmetic correction, not a substitute for dark
  calibration; single-pixel undersampled stars cannot always be distinguished
  from defects. Original CR2s are never modified.
- The first accepted frame initializes display levels; Low/Mid/High control a
  midtone transfer independently of saved linear samples. Both preview and FITS
  share the same colour-corrected linear data. Per-frame automatic white balance
  is never used, so colours do not drift as more frames are added.
- Local background/MAD noise estimation detects compact, unsaturated stars.
  Catalogs are spatially distributed and bounded. Alignment proposes canonical
  triangle matches and refines a similarity transform against unique matches.
- Final gates check at least 8 stars (configurable upward), match fraction,
  two-dimensional spatial coverage, scale/orientation, overlap of at least 25%,
  residual RMS and a conservative local chance-association check. Failed frames
  remain on disk and are **not** silently accumulated without registration.
- The first accepted frame defines the reference. Each later frame is sampled
  once using bilinear interpolation. Per-pixel coverage prevents blank edges
  from darkening the average.
- Accumulation uses double-precision running means and per-pixel variance.
  Optional online sigma rejection begins after five samples per pixel. It does
  not retroactively remove outliers in the initial samples. The default is 4σ.
- Disabling alignment uses identity registration; this is useful for fixed
  camera/bench validation but does not correct sky motion.
- A run with rejected frames ends with a warning and accepted/captured counts;
  a run with no accepted frames is an error and produces no empty FITS. A room
  scene is not a valid test of stellar registration: do not bypass its quality
  checks to force unrelated detections into an aligned stack.

### Horizontal banding

**Horizontal banding reduction** is optional and off by default. It operates on
each linear RGB frame before detection/registration and integration. Per-channel
row medians exclude saturated samples. A symmetric neighbourhood of rows, excluding
the row being measured, estimates the local background; only the residual row
offset is subtracted. The median offset is removed to retain the global pedestal.
Negative linear samples remain negative instead of being clipped to black.

- `horizontal_banding`: enable/disable for the next run.
- `banding_strength`: 0–1, default 1; blends the estimated correction.
- `banding_radius`: 2–256 rows, default 32; the local reference extends this
  distance above/below a row, shortened symmetrically near image boundaries.
- `banding_max_offset`: maximum absolute per-channel linear correction before
  strength, default 0.02 (2% of normalized full scale).

The first/last two rows are not estimated from an asymmetric window. Rows with
too few unclipped samples are left unchanged. Each JSON frame entry records the
correction RMS and the number of rows that hit the limit. This filter reduces
row offsets; it cannot distinguish every real horizontal structure from sensor
banding, remove vertical/column noise, or replace matched dark/bias calibration.
Smooth gradients and stellar contrast are covered by regression tests. Changing
these processing settings requires a new stack; original CR2s stay untouched.

The C++ geometry, spatial coverage and similarity-fit routines are adapted from
the [official N.I.N.A. LiveStack plugin](https://github.com/isbeorn/nina.plugin.livestack)
at revision `cb178bc562c54917a913568dc033a49ee5ded8cf`, under MPL-2.0.
[License and source attribution](../licenses/nina-livestack/NOTICE.md) accompany
the app. The detector, bounded triangle search, accumulator, FITS writer and
capture orchestration are AstroPhoto code. This is **not** the complete N.I.N.A.
plugin or its external runtime. The optional affine/quad recovery below is an
independent bounded adaptation, not the complete plugin pipeline.

## Calibration and background processing (0.8.0)

PC folder settings `bias_directory`, `dark_directory`, `flat_directory` and
`dark_flat_directory` accept 1 to 100000 matching CR2 files. Empty paths disable that
type. Arithmetic masters are built before the first shutter command. Model, ISO,
active dimensions and CFA must match. Dark exposure must match Light; dark-flat
exposure must match Flat. Dark scaling and temperature compensation are absent.

Sensor metadata black levels are removed before master arithmetic. Dark already
includes residual bias and is subtracted once; bias is used when no dark exists.
Flats require bias or dark-flats and are normalized per CFA colour after offset
subtraction. Saturated and near-zero flat responses fail validation. Signed float
Bayer calibration precedes white balance, float bilinear demosaic and the camera
RGB matrix; negative samples are retained. The uncalibrated path retains LibRaw
demosaicing and isolated hot-pixel correction. The calibrated path can build a defect map from matched darks (bias fallback)
and repair known hot/cold samples and narrow bad columns from unmasked same-CFA
neighbours. Selection uses calibration data, never bright science stars. The
existing cosmetic toggle controls it. Broad clusters and sensor edges can remain
unrepaired; JSON reports the map and per-frame correction counts. Non-RGB Bayer and patterned black layouts are rejected.

Optional `affine_alignment` refines existing triangle matches by affine least
squares, retaining geometry, overlap and residual gates, with bounded quad
recovery as described below. Optional `background_normalization` matches additive channel
backgrounds to the reference. Optional `gradient_removal` fits a robust linear
plane to 8 by 8 background tiles. Both default off and affect integrated samples.
They can alter extended emission; neither provides photometric scaling.

Batch median/percentile integration is not used: online sigma and optional Winsor
limits keep memory bounded. Real-sky validation remains outstanding.
See the development additions below for Drizzle, TIFF and optional ASTAP solving. Calibration arithmetic is tested
numerically and the sensor reader with real CR2s. Physical calibration sets were
not available. Fixed camera settings and serial acquisition remain required.

## Files and recovery

All new acquisitions use the PC's **local calendar date**, followed by the user's
session name. Previously saved directories are left intact. Single captures and
sequences use the date of each capture. A stack run keeps its starting date even
when it crosses midnight, so its originals and report remain together.

```text
<output>/YYYY-MM-DD/<session>/Light|Dark|Flat|Bias/<UTC timestamp>_<counter>_<type>.CR2
<output>/YYYY-MM-DD/<session>/live-stacked/<unique UTC run timestamp>/
    frames/Light_000001.CR2
    frames/Light_000002.CR2
    ...
    stack-linear.fits
    stack.json
```

RAW+JPEG companions, when enabled on the camera, are preserved too. Every run
gets a distinct directory. `stack.json` is atomically checkpointed after each
processed frame. It records the camera configuration/native values and labels,
processing settings, file list, accept/reject reasons, transform, residuals,
coverage, integration time and final output path. The final FITS is written to a
temporary file and atomically committed; a failed write never replaces a valid
final output with a partial file. Do not remove original frames after an abnormal
shutdown. Set `source_directory` to a CR2 folder (a prior run's `frames` folder)
and Start to rebuild into a new run without shutter commands. Originals are
referenced, never overwritten. Source metadata supplies exposure duration; mixed
model, ISO, exposure and geometry are rejected. This is explicit recovery, not
automatic restart of acquisition after a crash.

Pause finishes the current exposure, download, processing and mandatory delay,
then parks at a safe boundary. Resume continues with the next frame. Stop while
paused saves the partial result. Settings remain locked during pause.
`checkpoint_frames` defaults to 10; zero disables intermediate image writes.
Each interval atomically replaces `stack-checkpoint.fits` and records its frame
count in JSON. Final FITS saving is independent of that interval.

The final image is a **32-bit floating-point FITS**, three RGB planes, plus a
`COVERAGE` image extension containing each pixel's accepted sample count.
`NCOMBINE` records accepted images and `EXPTIME` their total exposure time. It is
RAW-derived linear data, **not a fabricated Canon CR2**. Low/Mid/High and preview
white balance never change these linear values. Empty/rejected-only runs retain
the originals and JSON but do not create a misleading empty FITS.

Memory use is bounded by the configured RAW budget (default 2048 MiB); large or
invalid files are refused. Each exposure also requires free disk space above the
configured reserve plus 512 MiB for the next RAW/final output.

## Android and automation

The dedicated `feature:livestack` Compose module provides Stack mode, live
accumulated preview, progress, all processing settings, presets, indefinite runs,
graceful Stop and display levels. It supports portrait and landscape. Images are
received as bounded binary JPEG previews; the phone does not save captures.
Backgrounding/disconnecting the phone does not stop a PC stack.
The phone's **Start Live / Stop Live** button switches the camera stream itself,
not merely the displayed image. It remains disabled during acquisition. Stopping
Live View manually is respected by polling and normal reconnection handling.

MCP and the authenticated WebSocket controller share the same worker:

- `update_settings`: `live_stack` object (all fields exposed by `get_control_schema`).
- `stack_control`: `action = start | pause | resume | stop`.
- `stack_stretch`: `low`, `mid`, `high`, optional `asinh` (display only; allowed while exposing).
- `get_state`: `state.live_stack`, progress, counters, diagnostics and revision.
- `update_ui`: `view = stack | capture | live` in the existing Image panel.
- `get_image`: `source = stack`; WebSocket uses `preview_stack` binary frames.

Command acknowledgement only confirms acceptance. Follow `state.live_stack`
until `active` is false and inspect `phase`, `detail` and `final_file`.

## Validation

Software tests cover alignment under rotation/subpixel translation and outliers,
rejection of reflections/unrelated or clustered catalogs, hot-pixel rejection,
linear precision, edge coverage, sigma rejection, FITS encoding, real CR2 decode,
bounded acquisition, Stop, shutter ambiguity, timeout, disk space, changed
settings, configuration/schema round trips and responsive Image-mode containment.

On 2026-11-01, all 265 Windows software tests passed. A Canon EOS 1100D and
USB-connected Pixel 8 Pro completed real three-RAW stacks at the existing camera
settings (first ISO 1600 / f/22 / 1 s; after the colour fix ISO 800 / f/9 / 1 s).
The final run accepted all three frames with zero rejections. Tests covered
Live View off/on, polling while deliberately stopped, binary phone preview,
display-level changes, fixed exposure limit, portrait/landscape, restored Live
View and an indefinite run stopped after one exposure. A direct request to
change camera parameters during stacking was rejected by the PC even when
bypassing disabled phone controls. Camera properties were unchanged; original
CR2s, 4290 × 2856 linear FITS and JSON reports were checked, including frozen
white balance and cosmetic-correction counts. The corrected real RAW preview
was visually inspected on both PC and phone.
The camera was pointed indoors: acquisition/remote operation was tested on real
hardware with alignment disabled. Star registration was tested numerically;
field validation on a real night sky is still required.

A subsequent five-exposure run was commanded through the native AstroPhoto MCP
connector, at unchanged ISO 800 / f/9 / 1 s, alignment disabled, banding enabled
(strength 1, radius 32, limit 0.02). All **5/5** RAWs were integrated, with zero
rejections, a 5-second integrated exposure and a 196,041,600-byte FITS. Per-frame
banding correction RMS was approximately 0.00021–0.00023 in normalized units,
with no limited rows. Original frames and per-frame metadata were inspected.

## Windows Night theme

Settings offers exactly **Dark**, **Light**, and **Night (red)**. Night mode uses
a final GPU luminance-to-red pass over the complete Windows client, including
images, histograms, logs and menus. It also requests dark red native caption
colours where Windows supports them. It does not recolour stored data, change
the camera, or apply a red theme to Android. Raw/live/stack image endpoints retain
their original colours; the UI-panel screenshot endpoint includes the red theme.
`night_theme` persists in configuration; selecting it disables `light_theme`
and vice versa. Legacy preferences without `night_theme` remain unchanged.

The actual GPU smoke output was checked for zero green/blue pixels. A native MCP
test confirmed that the stacked image payload was byte-for-byte identical before
and after enabling Night mode while the panel screenshot was red.


## Additions in Windows 0.8.0 / Android 0.3.0

These changes ship in release v0.8.0 with the Windows 0.8.0 MSI and Android 0.3.0 APK.

### Drizzle and rejection

`drizzle_scale` accepts 1 (off), 1.5, 2 or 3. `drizzle_pixfrac` accepts 0.1 to 1.
Each source RGB pixel becomes a shrunken drop, projected through the inverse
registration transform. Polygon clipping computes overlap with output pixels.
Contributions are weighted by area, consolidated once per frame, then integrated.
This is RGB drizzle after demosaicing, not Bayer/CFA drizzle or optical distortion
calibration. Subpixel dithers are needed for extra spatial information. No-dither
magnification cannot create new detail. Empty pixels retain zero coverage.

The output preserves normalized linear surface brightness: an aperture sum in
output-pixel units scales with the square of the enlargement. Account for that
when comparing integrated flux with native-resolution images. FITS records
DRIZSCAL and PIXFRAC, frame-count COVERAGE and floating-point WEIGHT extensions.
The configured memory budget is checked against output dimensions before large
allocations. Large 1100D stacks at 2x/3x require a substantially larger budget;
selecting a scale does not promise it will fit into the default 2048 MiB.

With `winsorized_rejection`, online limits clamp late outliers rather than dropping
the whole pixel. It requires rejection enabled and starts after five samples.
This is an online estimator, not iterative batch Winsorized sigma clipping,
median or percentile integration. Early outliers are not removed retroactively.

The drop/area approach follows the description in
[STScI's Drizzle documentation](https://hst-docs.stsci.edu/drizzpac/chapter-3-description-of-the-drizzle-algorithm/3-2-drizzle-concept).
The implementation here is original C++ and is not the complete DrizzlePac pipeline.

### Export and stellar statistics

`export_tiff` adds a 32-bit floating-point RGB TIFF. `export_xisf` adds an
uncompressed XISF 1.0 file with planar Float32 RGB samples and exposure/frame
metadata. Both are written atomically. It retains linear and negative
samples and does not bake the preview stretch into the image. The classic TIFF
writer has a 4 GiB limit; use FITS above it. It supplies RGB/sample-format metadata
but no ICC profile. FITS remains the main stack product and includes coverage.

Per-frame median HFD and Gaussian-moment FWHM are measured from unsaturated,
compact detected stars and recorded in JSON. Stack measurements are also recorded
and displayed in native-pixel units after accounting for drizzle scale. Zero
means no valid stellar measurement, not perfect focus. Small fixed measurement
apertures and undersampling limit these diagnostic estimates; they are not a
calibrated optical performance measurement.

### Dedicated calibration acquisition

`frame_type` selects Light, Dark, Flat, Bias or Dark-flat. Master acquisition
requires alignment, drizzle, background correction and science calibration off.
The desktop and Android selectors clear those incompatible options when choosing
a master type. Prepare the lens cap, flat illumination and suitable exposure
manually. A native sensor float Bayer mean is saved as master-TYPE.fits, with CFA,
ISO, exposure and frame count. Original CR2s remain in frames/. That CR2 folder
can be supplied to the calibration settings; loading prebuilt master FITS is not
implemented. The colour stack is an auxiliary display product for master runs.

### Astrometry

`solver_executable` optionally selects a locally installed ASTAP executable and
`solver_database` its star-catalog directory. `solver_fov` is image height in
degrees (zero requests automatic search); timeout is 5 to 300 seconds. The C++
worker invokes ASTAP directly, without a command shell, after saving the final
FITS. It validates the exit code, explicit solution flag and finite WCS matrix.
INI/WCS sidecars and the JSON astrometry result are kept beside the stack.
An incomplete/failed solve is never reported as a valid coordinate solution.

ASTAP and an appropriate catalog are external optional dependencies and are not
bundled. The official ASTAP executable and G05/W08 archives were downloaded and
validated locally. The real ASTAP process successfully solved a one-degree ESO/DSS
M45 FITS field (RA 56.7503, Dec +24.1166 degrees). The user's wide-angle Pixel sky
JPEG did not solve with the tested field-height estimate, even with W08 present;
a successful solve of that image or a fresh Canon sky stack is not claimed.
Automatic field-height search covers approximately 0.3 to 10 degrees; explicitly
supply the field height for wider images and use an appropriate database.
See the [ASTAP command-line reference](https://www.hnsky.org/astap.htm#astap_command_line).
No mount movements or pointing commands are implemented.

### Registration recovery

Affine refinement keeps the existing unique-match, coverage, overlap, plausible
scale/shear and RMS gates. If triangle matching fails, a bounded four-star search
uses affine-invariant barycentric descriptors to propose recovery transforms.
It is limited to 16 catalog stars and 4096 candidate trials. This is AstroPhoto's
bounded quad recovery, not the complete NINA plugin and not astrometric solving.
Real-sky field validation is still required.

### Recovery and sequence workflow

Optional `auto_rebuild` uses a saved recovery configuration for an interrupted
run to rebuild its retained CR2s on the next startup. It never resumes shutter
acquisition automatically. The recovery configuration is written atomically and
removed after a successful/graceful completion. It defaults off. Save preferences
to persist the startup choice. Missing originals or calibration files produce an
error rather than invented replacement frames.

`from_sequence` uses a homogeneous Light sequence plan: identical camera settings
and exposure mode, summed frame counts, the largest configured interval, and the
first row's initial delay. Camera properties are applied before stack preparation.
Heterogeneous plans are rejected before capture; split them into separate stacks.
Explicit CR2-folder rebuild and safe pause/resume are also available.

Only the EOS 1100D module is supplied, as requested. Additional camera model folders
can use the existing profile interface; other models and simultaneous multi-camera
control are not validated or advertised. Exposure settings remain fixed
within a stack; mixed exposures are rejected. Acquisition remains serialized with
a minimum two-second delay. Only the EOS 1100D has a validated hardware module.
No physical Bias/Dark/Flat sets were available, and the connected camera was indoors
because the sky was cloudy. Numerical tests and indoor capture do not replace
validation with calibration sets and an actual stellar field.

Integrated exposure totals use RAW EXIF shutter durations; requested camera timing
is retained separately for diagnostics and the live progress estimate.


### Display precision and experimental status

The desktop stack texture receives 16-bit RGBA values, reducing intermediate
stretch quantization. The final window framebuffer/monitor may still be 8-bit;
this is not an HDR display claim. Android receives bounded 8-bit JPEG previews.
Both clients offer MTF Low/Mid/High and an optional Asinh stretch; these never
change FITS, TIFF or XISF linear samples. Reset display restores MTF defaults.

**Experimental astronomical processing:** calibration, defect-map repair,
affine/quad registration, background correction, RGB drizzle, online rejection
and stellar statistics have numerical regression coverage but have not been
validated together on an actual Canon night-sky dataset. No physical calibration
sets were available, and the camera pointed indoors under cloudy weather.
The real five-RAW acquisition/replay tests validate pipeline operation, not
photometric accuracy, calibration quality or real-sky registration. ASTAP solving
has a separate real ESO/DSS field test, not a Canon field session.

| Requested area | Implemented behaviour and limits |
| --- | --- |
| Bias / Dark / Flat / Dark-flat | Matching CR2 folders, signed Bayer arithmetic, native Bayer master acquisition; real calibration-set validation pending |
| Registration | Similarity plus optional affine refinement and bounded quad recovery; gates reject invalid matches |
| Drizzle | Area-weighted RGB drops at 1.5x / 2x / 3x; dithers required for extra information; not CFA drizzle |
| Astrometry | Optional local ASTAP and catalog, checked WCS sidecars and JSON, failure warning; no mount control |
| Background / cosmetic correction | Additive background matching, robust linear gradient, calibration hot/cold/column map and isolated-hot-pixel filter |
| Rejection | Online sigma or Winsor limits after five samples; no retroactive batch median/percentile estimator |
| Pause / recovery / checkpoints | Safe pause/resume, CR2-folder replay, opt-in startup rebuild without new shutter commands, periodic atomic FITS |
| Planner | Homogeneous Light plans; heterogeneous settings require separate stacks |
| Export / display | Linear Float32 FITS, TIFF and XISF; desktop 16-bit texture, display-only MTF/Asinh, HFD/FWHM diagnostics |
| Safety constraints | One exposure/RAW job at a time, minimum 2 s interval, memory/disk limits and fixed exposure settings; these remain intentional |
| Camera support | Existing model-module architecture; only physically tested EOS 1100D profile supplied |

See [hardware validation](HARDWARE_VALIDATION.md) for exact test scope. A warning
or an experimental label is not proof that a failed solve or rejected frame was
usable; always inspect the report and retained RAWs.
