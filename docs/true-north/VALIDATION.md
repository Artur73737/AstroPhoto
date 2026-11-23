# Windows 0.10.2 validation - 2026-11-23

## Original supplied images: solved, not replaced

All three originals in demo_photo are processed afresh. No WCS sidecar, image-
specific coordinates, precomputed solution or generated star image is used.
The original PNG hashes are included in the installer example manifest.

| Input | Pixels | Solved width x height (degrees) | Unique catalogue matches | RMS (pixels) |
| --- | --- | --- | --- | --- |
| Screenshot 2026-11-22 184047.png | 2006 x 906 | 6.25868 x 2.82900 | 166 | 0.383009 |
| Screenshot 2026-11-22 184052.png | 1396 x 692 | 4.35512 x 2.16153 | 92 | 0.382545 |
| Screenshot 2026-11-22 184056.png | 832 x 542 | 2.59771 x 1.69256 | 25 | 0.269276 |

The fields contain valid catalogue stars. The original ASTAP quad search failed
to recover them; that was not evidence that the supplied images were incorrect.
The updated C++ polar workflow uses a fresh ASTAP centroid extraction followed by
scale-invariant Gaia pattern matching, unique one-to-one correspondences, bounded
hypotheses, robust affine refinement, spatial coverage and chance-match rejection.
It computes a TAN WCS from the accepted measurements. The general ASTAP solve
remains available when polar recovery does not validate a solution.

The image index is not an angular label. Width and height are measured from WCS;
the sources have approximately the same pixel scale and different raster fields.
The second image's J2000 pole is measurably off centre. No solver assumes that the
pole must equal the image centre. The purple point is the pole of the explicitly
declared projection epoch; it is distinct from the J2000 pole and Polaris.

This verifies the coordinate pipeline on the supplied rendered sky fields. RMS
is a catalogue-registration residual, not certified geographic bearing accuracy.
The images have no verified physical camera/site/exposure metadata. Therefore
plate-only output explicitly returns geographic_bearing=null and never auto-
confirms the section-3.6 contract.

## Software and regressions

The source tree separates feature include/astro/... headers, src implementations,
resources and tests. Boundary/layout checks and independent compilation of every
public header run with the build. The 341 software CTest entries have zero failures
(12 optional data cases are reported as skipped). Both ASI662MC hardware cases and
the extra real-file cases on all three user originals also pass. Pure numerical tests cover matching with
rotation, scale and parity, cancellation and unrelated-field rejection. No SDK is
replaced with a mock library.

The FITS pipeline numerically writes, reads and integrates five RAW16 frames at
valid gain zero. It checks accepted previews and the final scientific FITS/report.
A separate Bayer preview test verifies demosaicing instead of showing a green
sensor mosaic. These are numerical sensor fixtures, not ASI hardware exposures.
Three genuine DSS2 fields also pass the independent ASTAP/catalog regressions;
archive images do not establish the physical compass contract.

## Desktop and packaged runtime

The executable accepts --true-north-demo=1/2/3 as original-image indices. It opens
the corresponding supplied image, starts a fresh operation and exposes the actual
process phase, source-extraction/catalogue matching diagnostics and elapsed time.
The status is above the scrollable settings. Cancellation is bounded and old
coordinates are cleared before another solve. A failed solve never keeps a stale
marker. Solver scratch outputs go to a dedicated temporary directory.

The MSI includes the ASI SDK runtime, signed ZWO USB driver staging helper,
Canon EDSDK runtime, app-local Microsoft runtime, ASTAP and complete G05/W08 data.
Canon USB uses Windows native PTP. Data and driver installation do not imply a
successful physical ASI acquisition.

## Hardware and physical limits

The ASI662MC was attached for Windows 0.10.2 testing on 2026-11-23. Both opt-in
hardware cases passed: all writable controls accepted their current values with
readback, Gain was changed and restored, 24 real Live View JPEGs were decoded,
a RAW16 FITS exposure completed, and video restarted in the same session. The
application worker separately verified stream restart, decoding and native control
publication. SDK metadata supplies names, ranges, defaults, units, Auto support
and supported enumerated choices. Camera ownership must be exclusive.

This validates this camera's acquisition path, not every ASI model or sky-field
stacking. Colour conversion is neutral, not a calibrated sensor colour profile.
No levelling, attachment rigidity or clock accuracy can be certified from a
screenshot. Section 3.6 must be physically verified by the user before accepting
any geographic compass result.
