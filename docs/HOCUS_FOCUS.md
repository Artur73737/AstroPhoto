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
