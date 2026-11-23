#include <astro/features/true_north/astronomy/TrueNorth.hpp>
#include <astro/core/domain/Core.hpp>
#include <astro/stacking/astrometry/PlateSolve.hpp>
#include <astro/features/true_north/astrometry/PolarSolve.hpp>
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
using nlohmann::json;
int main(int argc, char** argv) {
    try {
        if (argc != 5) {
            std::cerr << "Usage: astro_true_north IMAGE observation.json ASTAP_EXE CATALOG_DIRECTORY\n";
            return 2;
        }
        std::ifstream f(argv[2]);
        auto j = json::parse(f);
        astro::north::Observation o;
        o.latitude = j.at("latitude");
        o.longitude = j.at("longitude");
        o.height = j.at("height_m");
        astro::north::utc(o, j.at("utc_mid_exposure"));
        o.physicalContractConfirmed = j.at("section_3_6_confirmed");
        o.rigid = j.at("rigid");
        o.mirrored = j.at("mirrored");
        o.inverted = j.at("inverted");
        o.roll = j.at("roll_degrees");
        o.levelingSigma = j.at("leveling_sigma_degrees");
        o.timeSigma = j.at("time_sigma_seconds");
        o.latitudeSigma = j.at("latitude_sigma_degrees");
        o.longitudeSigma = j.at("longitude_sigma_degrees");
        o.pressure = j.value("pressure_hpa", 1013.25);
        o.temperature = j.value("temperature_c", 10.);
        o.dut1 = j.value("dut1_seconds", 0.);
        o.humidity = j.value("relative_humidity", .5);
        o.solveSigma = j.value("solve_sigma_degrees", 10. / 3600);
        astro::north::Wcs w;
        w.width = j.at("width");
        w.height = j.at("height");
        w.crpix1 = (w.width + 1) * .5;
        w.crpix2 = (w.height + 1) * .5;
        w.cd = {-.001, 0, 0, .001};
        astro::north::validate(w, o);  // Reject physical contract before invoking solver.
        astro::StackOptions options;
        options.solverExecutable = argv[3];
        options.solverDatabase = argv[4];
        options.solverFov = j.at("field_height_degrees");
        auto solved = astro::north::solvePolarField(astro::pathFromUtf8(argv[1]), options);
        if(!solved.solved)solved=astro::stacking::solvePlate(astro::pathFromUtf8(argv[1]), options);
        if (!solved.solved) throw std::runtime_error(solved.detail);
        if (solved.width != w.width || solved.height != w.height)
            throw std::invalid_argument("Declared image dimensions disagree with ASTAP");
        auto& m = solved.wcs;
        w.crpix1 = m.at("CRPIX1");
        w.crpix2 = m.at("CRPIX2");
        w.ra = m.at("CRVAL1");
        w.dec = m.at("CRVAL2");
        w.cd = {m.at("CD1_1"), m.at("CD1_2"), m.at("CD2_1"), m.at("CD2_2")};
        auto result = astro::north::calculate(w, o);
        auto pixel = [](auto p) -> json { return p ? json{{"x_fits", p->x}, {"y_fits", p->y}} : json(nullptr); };
        json out = {{"centre_azimuth_degrees", result.centre.azimuth},
                    {"centre_altitude_degrees", result.centre.altitude},
                    {"north_horizon_pixel", pixel(result.northHorizon)},
                    {"celestial_pole_pixel", pixel(result.pole)},
                    {"north_image_bearing_degrees", result.northBearingInImage},
                    {"estimated_sigma_degrees", result.uncertaintyDegrees},
                    {"measured_roll_degrees", result.measuredRoll},
                    {"warnings", result.warnings}};
        out["corners"] = json::array();
        for (auto c : result.corners)
            out["corners"].push_back({{"azimuth", c.azimuth}, {"altitude", c.altitude}});
        std::cout << out.dump(2) << '\n';
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
