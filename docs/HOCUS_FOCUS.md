# Hocus Focus adaptation

AstroPhoto includes a native C++ adaptation of numerical components from
[Hocus Focus](https://github.com/ghilios/hocus-focus), by George Hilios.
The complete reference repository is downloaded locally in
`external/hocus-focus`, pinned to
`bced3c30d8a86114e2578039149667242737b942`. Restore or verify it with
`./scripts/fetch-hocus-focus.ps1`. It is a reference checkout, not a runtime
dependency. The upstream plugin targets NINA, .NET and WPF; AstroPhoto remains
C++ and Dear ImGui.

## Code adapted

| Upstream source | Native implementation and use |
| --- | --- |
| `StarDetection/WeightRegularization.cs` | Replace unknown uncertainty with the median positive uncertainty; floor small uncertainties to 20% of that median |
| `StarDetection/HyperbolicFittingAlglib.cs` | Symmetric four-parameter hyperbola and analytic derivatives |
| `StarDetection/AlglibHyperbolicFitting.cs` | Iterative Huber residual weighting, weighted goodness of fit, covariance and leave-one-out minimum stability |
| `StarDetection/StarDetector.cs` local background plane | Four robust plane-fitting iterations with Huber weights; subtract the fitted local background in stellar photometry |

These implementations live in `src/core/src/HocusFocusMath.cpp` and its public
header. The bounded nonlinear solver is implemented in C++; ALGLIB and NINA
binaries are not redistributed. The autofocus controller calls the fit on
actual measurements, and `StarAnalysis.cpp` calls the background estimator.

The review concentrated on these numerical sources, their upstream tests,
the autofocus engine's bracketing/retry/final-validation paths and the detector's
measurement paths. The entire upstream repository is present locally; this is
not a claim that every UI, simulator, tilt-wizard or unrelated source file has
received a complete semantic review.

## Canon-specific behavior

Every production run first calibrates the optical travel limits with large
commands: seek near, verify reverse response and return, traverse to far, verify
again, then return to near and check repeatability. The previous run's focus
position is never reused as the starting origin. Star detections are not needed
for this calibration: a normalized spatial image signature measures motion.
A blank/noisy field cannot prove a limit. When the signature or reverse response
is unobservable and automatic search is enabled, a bounded coarse survey takes
over. It measures after every large Near command (20 by default), then every
large Far command (40 by default), including positions with zero stars. These
counts are configurable search bounds, not proof of reaching either endpoint.
It retains a stable stellar field and compares paired HFD where identities
overlap. The return searches for that optical field again, approaches from Near
and remeasures it before starting refinement. Failed return, empty field and
budget exhaustion cannot promote the run. Limits remain explicitly unknown.

These are **optical estimates**, not physical encoder positions. The 1100D does
not provide the SDK focus-position-memory interface documented for newer RF
camera/lens combinations. A stationary image alone is insufficient: the lens
must respond to an opposite-direction probe and reproduce the endpoint. Slow
cumulative changes cannot qualify as a plateau. One full re-verification is
allowed if the near endpoint changes, within the original shared budgets.

When calibration succeeds, the large-drive sweep is bounded by its measured command range. Fine refinement
also watches for the endpoint image signatures. No conversion between large,
medium and small displacement is invented. Manual moves, zoom, property changes,
capture, disconnect and stopped Live View invalidate the calibration. Each new
AF start recalibrates regardless. Every queued motor command requires a newer
Live View frame than the previous command; a stalled stream cannot drain a queue.

Autofocus always measures **large -> medium -> small** drives, returning and
remeasuring between tiers. Scouting locates the near wing. A separate continuous
farward sweep supplies the fitted curve, so opposite approach directions are
not mixed. Each command size has its own coordinate origin and curve. Canon
drive commands have no absolute-position encoder or calibrated conversion ratio.

The precision fit uses only star identities present across the fitted points.
The final tier requires at least seven distinct measured positions, measurements
on both sides of the fitted minimum, adequate weighted R-squared and leave-one-out
stability. The measured paired-star improvement on both wings must also exceed
temporal noise: a perfect numerical fit to an imperceptibly shallow curve is
not sufficient. The predicted return is independently measured with additional
frames and compared with the measured minimum and completed tiers. A curve fit
alone cannot declare success. Unmeasurable coarse neighbors can bound a search
region, but cannot substitute for the final precision fit.

The graph uses actual relative command coordinates. Uncertainty shown during
sampling is the covariance estimate; after curve evaluation it is the standard
deviation of leave-one-out minima. Units are commands of the current drive, not
degrees or micrometers. `fit_min_r_squared` defaults to 0.85;
`fit_max_uncertainty` defaults to one sampling interval for coarse stages and
one smallest-drive command for the final stage, regardless of sampling spacing.
If the fine response spans less than three times its temporal scatter, spacing
grows fourfold instead of twofold, within the same movement/pass limits.
A fully widened fine sweep (or the last permitted pass) that remains below noise restores the measured best.
After the return is independently remeasured and checked against the completed
large/medium stages, it ends with a **warning** and unconfirmed fine precision.
A failed return or hardware fault remains an error. It does not repeatedly
recenter on random noise minima. An accepted SDK command is not proof of physical
lens displacement.

Small measurements use at least 11 frames per source by default and 22 on final
verification. A temporal half-window drift check waits for settling within a
bounded timeout rather than accepting a moving mean after a fixed sleep.
Before the small sweep, two stationary measurements estimate temporal noise.
An observable wing is sought with small commands, then Near1/Far1 reversals are
measured twice, each response confirmed by an independent stationary block.
The count includes both dead commands and commands needed for an optical change
above noise: it is a conservative **response bound**, not an exact backlash or
encoder reading. Measured compensation is used only when both repetitions agree
within one command and a fresh return reproduces the origin. Otherwise the
configured compensation remains in use. Missing stars on a probe cause a bounded
return instead of silently accepting a calibration.

The optional precision warning requires a fresh return no worse than the best
and completed tiers within the measured noise threshold (with a 1% floor).
`allow_precision_warning=false` keeps this outcome as an error. The measured
return HFD is reported separately from the still-unconfirmed small-stage result.
Diagnostics include per-observation HFD, scatter, delta, threshold, command count,
direction, frame count and elapsed settling time, exposed in native MCP and logs.
Response per command uses only consecutive one-way movements; mixed-direction
returns are marked direction zero and do not overwrite that estimate.

Defaults allow 600 total commands, eight passes and 600 seconds, shared with
calibration (160-command sublimit). Calibration uses five plateau confirmations,
a two-command reverse probe and a four-noise-unit comparison threshold.
The UI and MCP expose these parameters and the calibration's actual command
count, detected/unknown endpoints, estimated range and measured signal changes.

## Deliberate differences and limits

- AstroPhoto retains its background-subtracted **half-flux diameter**. The
  upstream `MeasureStar` implementation at this revision computes a
  flux-weighted mean radius. These are different metrics and must not be
  numerically equated.
- This port implements the symmetric model. Upstream hybrid/asymmetric model
  selection, PSF fitting, tilt analysis, the aberration inspector, NINA device
  orchestration and its settings wizard are not included.
- Acquisition from zero stars remains a bounded Canon motor search. It cannot
  guarantee stars exist in the frame, recover an arbitrary mechanically
  inaccessible focus position, or compensate for every lens's nonlinear motion.
- Live View JPEGs and a monitor scene are not equivalent to RAW exposures of
  real stars. Software tests do not establish optical accuracy.

See [hardware validation](HARDWARE_VALIDATION.md) for the physical trial results.

## License and reproducibility

The adapted numerical source files and adapted numerical tests retain the
upstream copyright notice and are licensed under **MPL-2.0**. The complete
license and attribution are in `licenses/hocus-focus`. Release packages include
the license, attribution and the covered numerical source/header beside the
executable. This preserves source availability for those covered files without
relicensing unrelated application files.

Tests cover exact analytic minima, reordered/translated coordinates, gross
outliers, flat and invalid data, uncertainty regularization, background gradients
and contamination. Controller tests independently cover the three tiers,
same-star comparisons, forward-only fitting, return verification, missing stars,
unresponsive motors and bounded cancellation/failure.
