#include <astro/features/true_north/astrometry/PolarSolve.hpp>
#include <astro/features/true_north/astrometry/catalog/PolarCatalog.hpp>
#include <astro/features/true_north/astrometry/sources/PolarSources.hpp>
#include <astro/features/true_north/astrometry/matching/PolarMatch.hpp>
#include <astro/imaging/preview/photos/PhotoPreview.hpp>
#include <cmath>
namespace astro::north {
stacking::PlateSolution solvePolarField(const std::filesystem::path& file, const StackOptions& options,
                                        const std::function<bool()>& cancelled,
                                        const std::function<void(const std::string&)>& progress) {
    try {
        auto image = decodePreview(file);
        int width = image.width, height = image.height;
        image.rgba.clear();
        auto reference = detail::catalog(pathFromUtf8(options.solverDatabase));
        auto sources = detail::extract(file, options, cancelled, progress);
        auto match =
            detail::recover(reference, sources, width, height, options.solverTimeoutSeconds, cancelled, progress);
        auto best = match.transform;
        double det = best.a * best.d - best.b * best.c;
        stacking::PlateSolution solution;
        solution.solved = true;
        solution.width = width;
        solution.height = height;
        solution.wcs = {{"CRPIX1", best.tx},      {"CRPIX2", best.ty},     {"CRVAL1", 0},
                        {"CRVAL2", 90},           {"CD1_1", best.d / det}, {"CD1_2", -best.b / det},
                        {"CD2_1", -best.c / det}, {"CD2_2", best.a / det}};
        solution.detail = "Gaia polar recovery: " + std::to_string(match.stars) + " unique stars, RMS " +
                          std::to_string(match.rms) + " px; fresh ASTAP centroids, no cached solution";
        if (progress) progress(solution.detail);
        return solution;
    } catch (const std::exception& e) {
        return {false, e.what(), {}};
    }
}
}  // namespace astro::north
