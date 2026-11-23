#define NOMINMAX
#include <windows.h>
#include <GL/gl.h>
#include <astro/features/true_north/workspace/ZwoWorkspace.hpp>
#include <astro/cameras/zwo/sdk/ZwoCamera.hpp>
#include <astro/features/true_north/astronomy/TrueNorth.hpp>
#include <astro/stacking/astrometry/PlateSolve.hpp>
#include <astro/features/true_north/astrometry/PolarSolve.hpp>
#include <astro/core/domain/Core.hpp>
#include <astro/imaging/preview/photos/PhotoPreview.hpp>
#include <astro/imaging/fits/FitsImage.hpp>
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>
#include <mutex>
#include <thread>
#include <deque>
#include <functional>
#include <future>
#include <fstream>
#include <map>
#include <sstream>
#include <chrono>
#include <atomic>
#include <algorithm>
#include <iomanip>
#include <ctime>
#include <cmath>
#include <nlohmann/json.hpp>
namespace astro::ui {
void loadDemo(int);
namespace {
struct State {
    std::shared_ptr<zwo::Frame> frame;
    std::uint64_t revision{};
    bool connected{}, live{};
};
std::shared_ptr<zwo::Frame> readFitsPreview(const std::filesystem::path& file) {
    if (file.extension() != ".fits" && file.extension() != ".fit" && file.extension() != ".fts") {
        auto hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        struct Com {
            HRESULT hr;
            ~Com() {
                if (SUCCEEDED(hr)) CoUninitialize();
            }
        } com{hr};
        auto image = decodePreview(file);
        auto frame = std::make_shared<zwo::Frame>();
        frame->width = image.width;
        frame->height = image.height;
        frame->format = 1;
        frame->bytes.resize(std::size_t(image.width) * image.height * 3);
        for (std::size_t i = 0; i < frame->bytes.size() / 3; ++i)
            for (int c = 0; c < 3; ++c)
                frame->bytes[i * 3 + c] = image.rgba[i * 4 + 2 - c];
        return frame;
    }
    std::ifstream input(file, std::ios::binary);
    if (!input) throw std::runtime_error("Cannot load FITS preview");
    std::map<std::string, double> header;
    std::size_t cards = 0;
    for (;;) {
        char card[81]{};
        if (!input.read(card, 80)) throw std::runtime_error("Truncated FITS preview");
        ++cards;
        std::string text(card, 80);
        auto key = text.substr(0, 8);
        key.erase(key.find_last_not_of(' ') + 1);
        if (key == "END") break;
        if (text[8] == '=') try {
                header[key] = std::stod(text.substr(10));
            } catch (...) {
            }
        if (cards > 8192) throw std::runtime_error("FITS header exceeds limit");
    }
    if (header.at("BITPIX") != 16) return {};
    int width = int(header.at("NAXIS1")), height = int(header.at("NAXIS2"));
    if (width < 1 || height < 1 || std::uint64_t(width) * height > 100000000)
        throw std::runtime_error("FITS preview exceeds limit");
    input.seekg((cards * 80 + 2879) / 2880 * 2880);
    auto frame = std::make_shared<zwo::Frame>();
    frame->width = width;
    frame->height = height;
    frame->format = 2;
    frame->bytes.resize(std::size_t(width) * height * 2);
    double zero = header.contains("BZERO") ? header.at("BZERO") : 0,
           scale = header.contains("BSCALE") ? header.at("BSCALE") : 1;
    for (std::size_t i = 0; i < frame->bytes.size(); i += 2) {
        unsigned char b[2];
        if (!input.read(reinterpret_cast<char*>(b), 2)) throw std::runtime_error("Truncated FITS pixels");
        double physical = short((unsigned(b[0]) << 8) | b[1]) * scale + zero;
        auto value = static_cast<unsigned short>(std::clamp(physical, 0., 65535.));
        auto sample = i / 2;
        auto destination = (std::size_t(height - 1 - int(sample / width)) * width + sample % width) * 2;
        frame->bytes[destination] = value & 255;
        frame->bytes[destination + 1] = value >> 8;
    }
    return frame;
}
struct Workspace {
    Mailbox* mailbox{};
    Config config;
    std::uint64_t captureCommand{}, captureRevision{};
    const zwo::Frame* demoImageUploaded{};
    GLuint texture{};
    std::uint64_t revision{};
    int tw{}, th{};
    std::string source, solver, catalog, utcText, message;
    double latitude{}, longitude{}, height{}, roll{}, levelSigma{.1}, timeSigma{1}, fov{}, pressure{1013.25},
        temperature{10}, latitudeSigma{1. / 3600}, longitudeSigma{1. / 3600}, solveSigma{10. / 3600}, dut1{},
        humidity{.5};
    bool contract{}, rigid{}, originalGeometry{}, metadataVerified{};
    std::shared_ptr<std::atomic<bool>> cancel = std::make_shared<std::atomic<bool>>(false);
    ~Workspace() {
        cancel->store(true);
        if (solving.valid()) solving.wait();
    }
    bool pendingNorthSolve{};
    std::future<std::string> solving;
    bool demoMode{}, plateOnly{true};
    std::optional<north::Pixel> solvedPole;
    std::mutex progressMutex;
    std::string progress;
    std::chrono::steady_clock::time_point solveStarted;
    int demoDegrees{};
    std::string reportJson;
    std::shared_ptr<zwo::Frame> loadedFrame;
    std::optional<north::Result> result;
    void solve() {
        auto& w = *this;
        w.result.reset();
        w.solvedPole.reset();
        w.reportJson.clear();
        if (!w.plateOnly && !w.metadataVerified) {
            w.message = "Verify site coordinates and exposure-midpoint UTC for this image";
            return;
        }
        if (!w.plateOnly && !w.originalGeometry) {
            w.message = "Confirm original image geometry before compass analysis";
            return;
        }
        auto source = w.source;
        auto solver = w.solver, catalog = w.catalog;
        auto o = north::Observation{};
        o.latitude = w.latitude;
        o.longitude = w.longitude;
        o.height = w.height;
        o.roll = w.roll;
        o.levelingSigma = w.levelSigma;
        o.timeSigma = w.timeSigma;
        o.pressure = w.pressure;
        o.temperature = w.temperature;
        o.dut1 = w.dut1;
        o.humidity = w.humidity;
        o.latitudeSigma = w.latitudeSigma;
        o.longitudeSigma = w.longitudeSigma;
        o.solveSigma = w.solveSigma;
        o.physicalContractConfirmed = w.contract;
        o.rigid = w.rigid;
        try {
            north::utc(o, w.utcText);
            north::Wcs dummy{1, 1, 0, 0, {-.001, 0, 0, .001}, 2, 2};
            if (!w.plateOnly) north::validate(dummy, o);
        } catch (const std::exception& e) {
            w.message = e.what();
            return;
        }
        auto fov = w.fov;
        const bool plateOnly = w.plateOnly;
        w.solvedPole.reset();
        w.solveStarted = std::chrono::steady_clock::now();
        {
            std::lock_guard lock(w.progressMutex);
            w.progress = "Starting fresh solve";
        }
        w.cancel->store(false);
        auto cancel = w.cancel;
        w.result.reset();
        w.reportJson.clear();
        try {
            w.loadedFrame = readFitsPreview(astro::pathFromUtf8(source));
            w.demoImageUploaded = nullptr;
        } catch (const std::exception& e) {
            w.message = e.what();
            return;
        }
        w.reportJson.clear();
        w.solving = std::async(std::launch::async, [=, &w]() {
            try {
                std::filesystem::path image = astro::pathFromUtf8(source);
                StackOptions options;
                options.solverExecutable = solver;
                options.solverDatabase = catalog;
                options.solverFov = fov;
                auto status=[&w](const std::string& phase){std::lock_guard lock(w.progressMutex);w.progress=phase;};
                auto stopped=[cancel]{return cancel->load();};
                auto solved=north::solvePolarField(image,options,stopped,status);
                if(!solved.solved&&!stopped())solved=stacking::solvePlate(image,options,stopped,status);
                if (!solved.solved) throw std::runtime_error(solved.detail);
                int width = solved.width, height = solved.height;
                if (width < 2 || height < 2) throw std::runtime_error("ASTAP did not return image dimensions");
                auto& m = solved.wcs;
                north::Wcs wc{m.at("CRPIX1"),
                              m.at("CRPIX2"),
                              m.at("CRVAL1"),
                              m.at("CRVAL2"),
                              {m.at("CD1_1"), m.at("CD1_2"), m.at("CD2_1"), m.at("CD2_2")},
                              width,
                              height};

                w.solvedPole = north::celestialPole(wc, o);
                auto field = north::fieldOfView(wc);
                if (plateOnly) {
                    auto pole = w.solvedPole;
                    w.reportJson = nlohmann::json{
                        {"mode", "plate_only"},{"solver_detail",solved.detail},
                        {"fov_width_deg", field[0]},
                        {"fov_height_deg", field[1]},
                        {"width", width},
                        {"height", height},
                        {"pole", pole ? nlohmann::json{{"x", pole->x}, {"y", pole->y}} : nlohmann::json(nullptr)},
                        {"geographic_bearing", nullptr},
                        {"physical_contract_verified",
                         false}}.dump();
                    std::ostringstream text;
                    text << "Solved field: " << field[0] << " x " << field[1] << " degrees.\n";
                    if (pole) text << "Pole of declared epoch: " << pole->x << ", " << pole->y << " FITS pixels.\n";
                    text << "Geographic compass withheld: site, UTC and physical section 3.6 are not verified.";
                    return text.str();
                }
                auto r = north::calculate(wc, o);
                w.result = r;
                auto pixel = [](auto p) -> nlohmann::json {
                    return p ? nlohmann::json{{"x", p->x}, {"y", p->y}} : nlohmann::json(nullptr);
                };
                w.reportJson = nlohmann::json{
                    {"azimuth", r.centre.azimuth},
                    {"altitude", r.centre.altitude},
                    {"uncertainty", r.uncertaintyDegrees},
                    {"pole", pixel(r.pole)},
                    {"north_horizon", pixel(r.northHorizon)},
                    {"width", width},
                    {"height",
                     height}}.dump();

                std::ostringstream text;
                text << "Optical azimuth: " << r.centre.azimuth << " deg; altitude: " << r.centre.altitude
                     << " deg\nEstimated uncertainty: " << r.uncertaintyDegrees << " deg\n"
                     << r.warnings;
                if (r.pole) text << "\nCelestial pole (FITS pixels): " << r.pole->x << ", " << r.pole->y;
                if (r.northHorizon)
                    text << "\nNorth horizon (FITS pixels): " << r.northHorizon->x << ", " << r.northHorizon->y;
                return text.str();
            } catch (const std::exception& e) {
                return std::string(e.what());
            }
        });
    }
    void upload(const State& s) {
        if (!s.frame || s.revision == revision) return;
        auto& f = *s.frame;
        int bpp = f.format == 2 ? 2 : f.format == 1 ? 3 : 1;
        if (f.bytes.size() != std::size_t(f.width) * f.height * bpp) return;
        double scale = std::min(1., 1600. / std::max(f.width, f.height));
        tw = std::max(1, int(f.width * scale));
        th = std::max(1, int(f.height * scale));
        std::vector<unsigned char> rgb(std::size_t(tw) * th * 3);
        double black = 0, white = 65535;
        if (bpp == 2) {
            std::vector<unsigned short> samples;
            for (std::size_t i = 0; i + 1 < f.bytes.size();
                 i += std::max(std::size_t(2), (f.bytes.size() / 32768) & ~std::size_t(1)))
                samples.push_back(static_cast<unsigned short>(f.bytes[i] | (unsigned(f.bytes[i + 1]) << 8)));
            std::sort(samples.begin(), samples.end());
            if (!samples.empty()) {
                black = samples[samples.size() / 100];
                white = std::max(black + 1., double(samples[samples.size() * 999 / 1000]));
            }
        }
        for (int y = 0; y < th; ++y)
            for (int x = 0; x < tw; ++x) {
                auto p = (std::size_t(y * f.height / th) * f.width + x * f.width / tw) * bpp;
                for (int c = 0; c < 3; ++c)
                    rgb[(std::size_t(y) * tw + x) * 3 + c] =
                        bpp == 2
                            ? static_cast<unsigned char>(std::clamp(
                                  ((f.bytes[p] | (unsigned(f.bytes[p + 1]) << 8)) - black) / (white - black) * 255., 0.,
                                  255.))
                        : bpp == 3 ? f.bytes[p + 2 - c]
                                   : f.bytes[p];
            }
        if (!texture) glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, tw, th, 0, GL_RGB, GL_UNSIGNED_BYTE, rgb.data());
        revision = s.revision;
    }
};
}  // namespace
Workspace& workspace() {
    static Workspace w;
    return w;
}
std::filesystem::path applicationFolder() {
    wchar_t path[32768];
    auto count = GetModuleFileNameW(nullptr, path, 32768);
    if (!count || count == 32768) throw std::runtime_error("Cannot locate application folder");
    return std::filesystem::path(path).parent_path();
}
void loadDemo(int n) {
    if (n < 1 || n > 3) throw std::invalid_argument("Demo field must be 1, 2 or 3 degrees");
    auto& w = workspace();
    if (w.solving.valid() || w.pendingNorthSolve) throw std::logic_error("True North is busy");
    auto root = applicationFolder();
    const char* names[] = {"Screenshot 2026-11-22 184047.png", "Screenshot 2026-11-22 184052.png",
                           "Screenshot 2026-11-22 184056.png"};
    auto file = root / "examples" / "true-north" / names[n - 1];
    if (!std::filesystem::exists(file)) file = std::filesystem::current_path() / "demo_photo" / names[n - 1];
    w.result.reset();
    w.solvedPole.reset();
    w.reportJson.clear();
    w.loadedFrame.reset();
    w.tw = w.th = 0;
    w.source = file.string();
    w.solver = (root / "dependencies" / "astap" / "astap.exe").string();
    w.catalog = (root / "dependencies" / "astap").string();
    if (!std::filesystem::exists(w.solver)) {
        w.solver = (std::filesystem::current_path() / "external" / "astap" / "astap.exe").string();
        w.catalog = (std::filesystem::current_path() / "external" / "astap").string();
    }
    w.demoMode = true;
    w.plateOnly = true;
    w.demoDegrees = n;
    w.utcText = "2026-11-22T00:00:00Z";  // Declared projection epoch only, never claimed as exposure metadata.
    w.fov = 0;
    w.contract = w.rigid = w.originalGeometry = w.metadataVerified = false;
    w.message = "User-supplied planetarium screenshot: fresh solve required; geographic compass disabled";
}

void bindNorthCamera(Mailbox& mailbox, const Config& config) {
    auto& w = workspace();
    w.mailbox = &mailbox;
    w.config = config;
}
void zwoNorthWorkspace() {
    auto& w = workspace();
    State s;
    Snapshot snapshot;
    if (w.mailbox) {
        snapshot = w.mailbox->snapshot();
        s.connected = snapshot.connected && snapshot.cameraModule.id == "zwo_asi";
        s.live = snapshot.liveView;
        if (s.connected && !snapshot.frame.jpeg.empty() && snapshot.frame.revision != w.revision) {
            auto pixels = decodeImage(snapshot.frame.jpeg);
            s.frame = std::make_shared<zwo::Frame>();
            s.frame->width = pixels.width;
            s.frame->height = pixels.height;
            s.frame->format = 1;
            s.frame->bytes.resize(std::size_t(pixels.width) * pixels.height * 3);
            for (std::size_t i = 0; i < s.frame->bytes.size() / 3; ++i)
                for (int c = 0; c < 3; ++c)
                    s.frame->bytes[i * 3 + c] = pixels.rgba[i * 4 + 2 - c];
            s.revision = snapshot.frame.revision;
        }
    }
    if (w.loadedFrame) {
        State shown = s;
        shown.frame = w.loadedFrame;
        shown.revision = 1000000 + w.revision + 1;
        if (w.demoImageUploaded != w.loadedFrame.get()) {
            w.upload(shown);
            w.demoImageUploaded = w.loadedFrame.get();
        }
    } else if (w.solving.valid() || !w.loadedFrame)
        w.upload(s);
    if (w.solving.valid() && w.solving.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
        w.message = w.solving.get();
    const bool busy = w.solving.valid() || w.pendingNorthSolve;

    ImGui::TextWrapped(
        "True North compass: geographic north, not mount polar alignment. File input works independently; live input "
        "requires a ZWO ASI camera.");
    if (busy) {
        {
            std::lock_guard lock(w.progressMutex);
            ImGui::TextWrapped("%s", w.progress.c_str());
        }
        ImGui::Text("Elapsed %.1f s",
                    std::chrono::duration<double>(std::chrono::steady_clock::now() - w.solveStarted).count());
        ImGui::SameLine();
        if (ImGui::Button("Cancel solve")) w.cancel->store(true);
    } else if (!w.message.empty())
        ImGui::TextWrapped("%s", w.message.c_str());
    const float unit = ImGui::GetFontSize();
    const bool narrow = ImGui::GetContentRegionAvail().x < unit * 65;
    ImGui::BeginChild("ZWO and north controls", {narrow ? 0.f : unit * 30, narrow ? unit * 24 : 0.f},
                      ImGuiChildFlags_Borders);
    ImGui::PushItemWidth(unit * 12);
    ImGui::TextWrapped(
        "Select ZWO ASI in Connection. Live View, capture, native controls and stacking share the same core camera "
        "owner.");
    ImGui::SeparatorText("True North compass");
    ImGui::BeginDisabled(busy);
    for (int n : {1, 2, 3}) {
        ImGui::PushID(n);
        if (ImGui::Button(("User image " + std::to_string(n)).c_str())) {
            loadDemo(n);
            w.solve();
        }
        ImGui::PopID();
    }
    if (w.demoMode) {
        ImGui::TextWrapped(
            "User-provided planetarium screenshot. FOV is measured only after a successful solve. No physical compass "
            "validation.");
        if (ImGui::Button("Exit demo / real observation")) {
            w.demoMode = false;
            w.contract = w.rigid = w.metadataVerified = w.originalGeometry = false;
            w.loadedFrame.reset();
            w.result.reset();
        }
    }
    ImGui::EndDisabled();
    ImGui::BeginDisabled(busy);
    ImGui::Checkbox("Plate solve only (no geographic bearing)", &w.plateOnly);
    ImGui::InputText("Image file", &w.source);
    ImGui::InputText("ASTAP executable", &w.solver);
    ImGui::InputText("ASTAP catalog", &w.catalog);
    ImGui::InputDouble("FOV height hint (0 = auto)", &w.fov);
    ImGui::InputText(w.plateOnly ? "Declared projection epoch UTC" : "UTC exposure midpoint", &w.utcText);
    if (!w.plateOnly) {
        ImGui::InputDouble("Latitude (north +)", &w.latitude);
        ImGui::InputDouble("Longitude (east +)", &w.longitude);
        ImGui::InputDouble("WGS84 height (m)", &w.height);

        ImGui::InputDouble("Known roll (degrees)", &w.roll);
        ImGui::InputDouble("Level uncertainty (degrees)", &w.levelSigma);
        ImGui::InputDouble("Clock uncertainty (seconds)", &w.timeSigma);
        ImGui::InputDouble("Pressure (hPa)", &w.pressure);
        ImGui::InputDouble("Temperature (C)", &w.temperature);
        ImGui::InputDouble("Relative humidity (0..1)", &w.humidity);
        ImGui::InputDouble("DUT1 UT1-UTC (s)", &w.dut1);
        ImGui::InputDouble("Latitude uncertainty (deg)", &w.latitudeSigma, 0, 0, "%.6f");
        ImGui::InputDouble("Longitude uncertainty (deg)", &w.longitudeSigma, 0, 0, "%.6f");
        ImGui::InputDouble("Solve uncertainty (deg)", &w.solveSigma, 0, 0, "%.6f");
        ImGui::TextWrapped(
            "Section 3.6: physically level the base and sensor, landscape upright, no mirroring, roll zero +/- 0.1 "
            "degree. "
            "Keep camera rigid after leveling. Software cannot verify this physical condition.");
        if (w.demoMode)
            ImGui::TextWrapped("Planetarium input: physical orientation and observer metadata are not verified.");
        else {
            ImGui::Checkbox("I verified section 3.6 physically", &w.contract);
            ImGui::Checkbox("Camera and levelled base remained rigid", &w.rigid);
            ImGui::Checkbox("Original geometry: no undeclared crop / resize / flip", &w.originalGeometry);
            ImGui::Checkbox("Site and UTC midpoint describe this image", &w.metadataVerified);
        }
    }
    if (ImGui::Button("Solve loaded photograph")) w.solve();
    ImGui::BeginDisabled(!s.connected || busy);
    if (ImGui::Button("Capture and solve ZWO frame")) {
        if (!w.plateOnly && (!w.contract || !w.rigid || !w.originalGeometry || !w.metadataVerified))
            w.message = "Verify physical section 3.6 and observer metadata first";
        else {
            Command command;
            command.action = Action::CaptureSingle;
            command.config = w.config;
            command.config.singleCapture.count = 1;
            command.config.singleCapture.settings.clear();
            w.solveStarted = std::chrono::steady_clock::now();
            {
                std::lock_guard lock(w.progressMutex);
                w.progress = "Waiting for core RAW16 capture and download";
            }
            w.captureRevision = snapshot.lastCapture.revision;
            w.captureCommand = w.mailbox->submit(std::move(command));
            w.pendingNorthSolve = true;
            w.message = "Core capture requested; video is suspended safely for exposure";
        }
    }
    if (w.pendingNorthSolve && w.mailbox) {
        auto response = w.mailbox->result(w.captureCommand);
        if (response && response->finished && !response->error.empty()) {
            w.pendingNorthSolve = false;
            w.message = response->error;
        } else if (snapshot.lastCapture.revision > w.captureRevision && !snapshot.lastCapture.files.empty()) {
            w.pendingNorthSolve = false;
            w.source = snapshot.lastCapture.files.front().string();
            try {
                auto metadata = fits::read(snapshot.lastCapture.files.front(), 512ull * 1024 * 1024).metadata;
                if (metadata.contains("FLIP") && metadata.at("FLIP") != "0")
                    throw std::invalid_argument("Disable ASI image flip before physical True North capture");
                w.utcText = metadata.at("DATE-AVG");
                w.timeSigma = std::max(1., w.timeSigma);
                w.solve();
            } catch (const std::exception& e) {
                w.message = e.what();
            }
        }
    }

    ImGui::EndDisabled();
    ImGui::EndDisabled();
    ImGui::PopItemWidth();
    ImGui::EndChild();
    if (!narrow) ImGui::SameLine();
    ImGui::BeginChild("ASI preview", {0, 0}, ImGuiChildFlags_Borders);
    if (w.texture && w.tw > 0) {
        auto size = ImGui::GetContentRegionAvail();
        float scale = std::min(size.x / w.tw, size.y / w.th);
        auto pos = ImGui::GetCursorScreenPos();
        ImVec2 imageSize{w.tw * scale, w.th * scale};
        ImGui::Image(static_cast<ImTextureID>(w.texture), imageSize);
        auto* draw = ImGui::GetWindowDrawList();
        if (w.demoMode)
            draw->AddText({pos.x + 8, pos.y + 8}, IM_COL32(255, 200, 70, 255),
                          "PLANETARIUM INPUT - NOT A PHYSICAL COMPASS VALIDATION");
        if (!w.solving.valid() && w.loadedFrame) {
            ImVec2 centre{pos.x + imageSize.x * .5f, pos.y + imageSize.y * .5f};
            auto mark = [&](const std::optional<north::Pixel>& point, const char* label, ImU32 color) {
                if (!point) return;
                double dx = (point->x - (w.loadedFrame->width + 1) * .5) * imageSize.x / w.loadedFrame->width,
                       dy = -(point->y - (w.loadedFrame->height + 1) * .5) * imageSize.y / w.loadedFrame->height;
                double k = std::min({1., imageSize.x * .43 / std::max(.001, std::abs(dx)),
                                     imageSize.y * .43 / std::max(.001, std::abs(dy))});
                ImVec2 end{centre.x + float(k * dx), centre.y + float(k * dy)};
                draw->AddLine(centre, end, color, 2);
                draw->AddCircleFilled(end, 7, color, 24);
                draw->AddCircle(end, 9, IM_COL32(0, 0, 0, 255), 24, 2);
                draw->AddText({end.x + 8, end.y}, color, label);
                if (k < 1)
                    draw->AddText({end.x + 8, end.y + ImGui::GetFontSize()}, color, "OUTSIDE FRAME - direction only");
            };
            if (w.solvedPole) {
                draw->AddCircle(centre, 5, IM_COL32(255, 255, 255, 255), 16, 1);
                draw->AddText({centre.x + 8, centre.y + 12}, IM_COL32(255, 255, 255, 255), "IMAGE CENTER");
            }
            if (w.result) mark(w.result->northHorizon, "TRUE NORTH HORIZON", IM_COL32(255, 80, 80, 255));
            mark(w.solvedPole, "CELESTIAL AXIS / POLE", IM_COL32(225, 40, 255, 255));
        }
        ImGui::TextDisabled("Display preview. Purple: celestial axis. White: image centre. Original files remain unchanged.");
    } else
        ImGui::TextWrapped(
            "ZWO preview appears after starting its stream. Canon control remains in its existing panels.");
    ImGui::EndChild();
}
std::string zwoNorthControl(const std::string& request) {
    using J = nlohmann::json;
    auto p = J::parse(request);
    auto action = p.at("action").get<std::string>();
    auto& w = workspace();
    if (w.solving.valid() && w.solving.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
        w.message = w.solving.get();
    if (action == "demo") {
        loadDemo(p.at("degrees"));
        if (p.value("run", true)) w.solve();
    } else if (action == "solve") {
        if (w.solving.valid() || w.pendingNorthSolve) throw std::logic_error("True North is busy");
        if (p.contains("options")) {
            auto& o = p.at("options");
            if (!o.is_object()) throw std::invalid_argument("True North options must be an object");
            for (auto i = o.begin(); i != o.end(); ++i) {
                auto key = i.key();
                if (key == "file")
                    w.source = i.value().get<std::string>();
                else if (key == "solver")
                    w.solver = i.value().get<std::string>();
                else if (key == "catalog")
                    w.catalog = i.value().get<std::string>();
                else if (key == "epoch_utc")
                    w.utcText = i.value().get<std::string>();
                else if (key == "fov")
                    w.fov = i.value().get<double>();
                else if (key == "plate_only")
                    w.plateOnly = i.value().get<bool>();
                else if (key == "latitude")
                    w.latitude = i.value().get<double>();
                else if (key == "longitude")
                    w.longitude = i.value().get<double>();
                else if (key == "height_m")
                    w.height = i.value().get<double>();
                else if (key == "roll_deg")
                    w.roll = i.value().get<double>();
                else if (key == "physical_contract")
                    w.contract = i.value().get<bool>();
                else if (key == "rigid")
                    w.rigid = i.value().get<bool>();
                else if (key == "original_geometry")
                    w.originalGeometry = i.value().get<bool>();
                else if (key == "metadata_verified")
                    w.metadataVerified = i.value().get<bool>();
                else
                    throw std::invalid_argument("Unknown True North option: " + key);
            }
            w.demoMode = false;
        }
        w.solve();
    } else if (action != "state")
        throw std::invalid_argument("Unknown True North control action");
    auto s = w.mailbox ? w.mailbox->snapshot() : Snapshot{};
    J result = {{"ok", true},
                {"busy", w.solving.valid() || w.pendingNorthSolve},
                {"demo", w.demoMode},
                {"degrees", w.demoDegrees},
                {"source", w.source},
                {"message", w.message},
                {"zwo_connected", s.connected && s.cameraModule.id == "zwo_asi"}};
    {
        std::lock_guard lock(w.progressMutex);
        result["phase"] = w.progress;
    }
    if (w.solving.valid())
        result["elapsed_seconds"] =
            std::chrono::duration<double>(std::chrono::steady_clock::now() - w.solveStarted).count();
    if (!w.solving.valid() && !w.reportJson.empty()) result["result"] = J::parse(w.reportJson);
    return result.dump();
}
}  // namespace astro::ui
