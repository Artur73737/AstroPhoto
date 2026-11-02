N.I.N.A. LiveStack C++ adaptation

Source: https://github.com/isbeorn/nina.plugin.livestack
Revision: cb178bc562c54917a913568dc033a49ee5ded8cf
License: Mozilla Public License 2.0 (included).

NinaGeometry.cpp adapts AlignmentGeometry.cs and the spatial coverage and similarity fit from ImageTransformer2.Alignment.cs. Original authors: N.I.N.A. contributors. The remaining detector, triangle search, integration and RAW pipeline are AstroPhoto implementations; this is not a complete port of the N.I.N.A. plugin. Modified MPL source is distributed alongside the application.

Version 0.8.0 adds an original affine least-squares fit to NinaGeometry.cpp and
bounded barycentric quad recovery in the AstroPhoto registration code. These do
not reproduce the complete N.I.N.A. registration or solver pipeline.
