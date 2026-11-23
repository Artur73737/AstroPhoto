#include <astro/cameras/zwo/sdk/ZwoCamera.hpp>
#include <ASICamera2.h>
#include <astro/imaging/fits/FitsImage.hpp>
#include <thread>
#include <stdexcept>
#include <algorithm>
#include <fstream>
#include <limits>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
namespace astro::zwo {
namespace {
void check(ASI_ERROR_CODE e, const char* action) {
    if (e != ASI_SUCCESS) throw std::runtime_error(std::string("ZWO ") + action + ": SDK error " + std::to_string(e));
}
Device info(const ASI_CAMERA_INFO& c) {
    Device d{c.CameraID, int(c.MaxWidth),          int(c.MaxHeight),
             c.BitDepth, c.IsColorCam == ASI_TRUE, c.IsCoolerCam == ASI_TRUE,
             c.Name};
    for (int n : c.SupportedBins) {
        if (!n) break;
        d.bins.push_back(n);
    }
    for (auto n : c.SupportedVideoFormat) {
        if (n == ASI_IMG_END) break;
        d.formats.push_back(int(n));
    }
    if (d.color) {
        const char* patterns[] = {"RGGB", "BGGR", "GRBG", "GBRG"};
        if (int(c.BayerPattern) >= 0 && int(c.BayerPattern) < 4) d.bayer = patterns[int(c.BayerPattern)];
    }
    return d;
}
}  // namespace
struct Camera::Impl {
    int id{-1}, width{}, height{}, bin{1}, flip{}, x{}, y{};
    ASI_IMG_TYPE format{ASI_IMG_RAW16};
    bool video{}, exposing{};
    std::chrono::system_clock::time_point exposureStart;
    double exposureSeconds{};
    Device device;
    std::thread::id owner{std::this_thread::get_id()};
    void thread() {
        if (owner != std::this_thread::get_id()) throw std::logic_error("ZWO ownership thread violation");
    }
    void ready() {
        thread();
        if (id < 0) throw std::logic_error("ZWO not connected");
    }
    Frame buffer() {
        std::size_t bpp = format == ASI_IMG_RAW16 ? 2 : format == ASI_IMG_RGB24 ? 3 : 1;
        std::size_t n = std::size_t(width) * height * bpp;
        if (!n || n > 512ull * 1024 * 1024 || n > std::numeric_limits<long>::max())
            throw std::runtime_error("ZWO frame exceeds 512 MiB transfer bound");
        return {width, height, int(format), std::vector<std::uint8_t>(n)};
    }
};
Camera::Camera() : p(std::make_unique<Impl>()) {}
Camera::~Camera() {
    try {
        close();
    } catch (...) {
    }
}
std::vector<Device> Camera::enumerate() {
    std::vector<Device> list;
    for (int i = 0, n = ASIGetNumOfConnectedCameras(); i < n; ++i) {
        ASI_CAMERA_INFO c{};
        check(ASIGetCameraProperty(&c, i), "enumerate");
        list.push_back(info(c));
    }
    return list;
}
void Camera::open(int id) {
    p->thread();
    if (p->id >= 0) throw std::logic_error("ZWO already open");
    check(ASIOpenCamera(id), "open");
    p->id = id;
    try {
        check(ASIInitCamera(id), "initialize");
        ASI_CAMERA_INFO c{};
        check(ASIGetCameraPropertyByID(id, &c), "camera information");
        p->device = info(c);
        check(ASIGetROIFormat(id, &p->width, &p->height, &p->bin, &p->format), "read ROI");
        check(ASIGetStartPos(id, &p->x, &p->y), "read ROI origin");
        for (auto& c : controls())
            if (c.id == int(ASI_FLIP)) p->flip = int(c.value);
    } catch (...) {
        close();
        throw;
    }
}
void Camera::close() {
    p->thread();
    if (p->id < 0) return;
    abort();
    if (p->video) stream(false);
    check(ASICloseCamera(p->id), "close");
    p->id = -1;
}
bool Camera::connected() const {
    p->thread();
    return p->id >= 0;
}
Device Camera::device() const {
    p->ready();
    auto device = p->device;
    if (device.color && device.bayer.size() == 4) {
        auto pattern = device.bayer;
        for (int y = 0; y < 2; ++y)
            for (int x = 0; x < 2; ++x) {
                int sx = x + p->x, sy = y + p->y;
                if (p->flip & 1) sx = p->width - 1 - sx;
                if (p->flip & 2) sy = p->height - 1 - sy;
                device.bayer[y * 2 + x] = pattern[(sy & 1) * 2 + (sx & 1)];
            }
    }
    return device;
}
std::vector<Control> Camera::controls() {
    p->ready();
    int count;
    check(ASIGetNumOfControls(p->id, &count), "control count");
    std::vector<Control> out;
    for (int i = 0; i < count; ++i) {
        ASI_CONTROL_CAPS c{};
        check(ASIGetControlCaps(p->id, i, &c), "control capabilities");
        long v;
        ASI_BOOL a;
        check(ASIGetControlValue(p->id, c.ControlType, &v, &a), "read control");
        out.push_back({int(c.ControlType), c.Name, c.Description, c.MinValue, c.MaxValue, v, a == ASI_TRUE,
                       c.IsAutoSupported == ASI_TRUE, c.IsWritable == ASI_TRUE});
        auto& control = out.back();
        control.defaultValue = c.DefaultValue;
        switch (c.ControlType) {
            case ASI_EXPOSURE:
                control.unit = "us";
                control.logarithmic = true;
                break;
            case ASI_WB_R: case ASI_WB_B: case ASI_OFFSET: case ASI_GAMMA:
                control.category = "Image controls";
                break;
            case ASI_AUTO_MAX_EXP:
                control.category = "Auto exposure and miscellaneous";
                control.unit = std::string(c.Description).find("ms") != std::string::npos ? "ms" : "us";
                control.logarithmic = true;
                break;
            case ASI_AUTO_MAX_GAIN: case ASI_AUTO_TARGET_BRIGHTNESS: case ASI_MONO_BIN:
                control.category = "Auto exposure and miscellaneous";
                break;
            case ASI_TEMPERATURE:
                control.category = "Temperature and cooling";
                control.unit = "C";
                control.displayScale = .1;
                break;
            case ASI_TARGET_TEMP:
                control.category = "Temperature and cooling";
                control.unit = "C";
                break;
            case ASI_COOLER_ON: case ASI_FAN_ON: case ASI_COOLER_POWER_PERC: case ASI_FAN_ADJUST:
                control.category = "Temperature and cooling";
                break;
            case ASI_FLIP:
                for (int i = 0; i < 4; ++i)
                    if (i >= c.MinValue && i <= c.MaxValue)
                        control.choices.push_back({i, std::vector<std::string>{"None", "Horizontal", "Vertical", "Both"}[i]});
                break;
            default: break;
        }
        switch (c.ControlType) {
            case ASI_HARDWARE_BIN: case ASI_HIGH_SPEED_MODE: case ASI_MONO_BIN:
            case ASI_COOLER_ON: case ASI_FAN_ON:
                if (c.MinValue == 0 && c.MaxValue == 1) control.choices = {{0, "Off"}, {1, "On"}};
                break;
            default: break;
        }
    }
    return out;
}
void Camera::set(int id, long value, bool automatic) {
    p->ready();
    if (p->exposing) throw std::logic_error("Controls locked during still exposure");
    auto all = controls();
    auto c = std::find_if(all.begin(), all.end(), [&](auto& c) { return c.id == id; });
    if (c == all.end() || !c->writable || value < c->minimum || value > c->maximum || (automatic && !c->autoSupported))
        throw std::invalid_argument("Unsupported ZWO control value / auto mode");
    check(ASISetControlValue(p->id, ASI_CONTROL_TYPE(id), value, automatic ? ASI_TRUE : ASI_FALSE), "set control");
    long actual;
    ASI_BOOL autoMode;
    check(ASIGetControlValue(p->id, ASI_CONTROL_TYPE(id), &actual, &autoMode), "verify control");
    if (id == int(ASI_FLIP)) p->flip = int(actual);
}
void Camera::roi(int w, int h, int bin, int format, int x, int y) {
    p->ready();
    if (p->video || p->exposing) throw std::logic_error("Stop acquisition before ROI change");
    auto& d = p->device;
    if (w <= 0 || h <= 0 || x < 0 || y < 0 || w % 8 || h % 2 ||
        std::find(d.bins.begin(), d.bins.end(), bin) == d.bins.end() ||
        std::find(d.formats.begin(), d.formats.end(), format) == d.formats.end() || w > d.width / bin ||
        h > d.height / bin || x > d.width / bin - w || y > d.height / bin - h)
        throw std::invalid_argument("Invalid ZWO ROI/bin/format");
    check(ASISetROIFormat(p->id, w, h, bin, ASI_IMG_TYPE(format)), "set ROI");
    check(ASIGetROIFormat(p->id, &p->width, &p->height, &p->bin, &p->format), "verify ROI");
    check(ASISetStartPos(p->id, x, y), "set ROI position");
    check(ASIGetStartPos(p->id, &p->x, &p->y), "verify ROI origin");
}
void Camera::stream(bool on) {
    p->ready();
    if (p->exposing) throw std::logic_error("Still exposure in progress");
    if (on == p->video) return;
    check(on ? ASIStartVideoCapture(p->id) : ASIStopVideoCapture(p->id), on ? "start stream" : "stop stream");
    p->video = on;
}
Frame Camera::frame(int ms) {
    p->ready();
    if (!p->video || ms < 0 || ms > 1000) throw std::invalid_argument("Stream required; timeout 0..1000 ms");
    auto f = p->buffer();
    auto e = ASIGetVideoData(p->id, f.bytes.data(), long(f.bytes.size()), ms);
    if (e == ASI_ERROR_TIMEOUT) return {};
    check(e, "video frame");
    return f;
}
double Camera::fixedExposureSeconds() {
    p->ready();
    long value;
    ASI_BOOL automatic;
    check(ASIGetControlValue(p->id, ASI_EXPOSURE, &value, &automatic), "read exposure timing");
    if (automatic == ASI_TRUE || value <= 0)
        throw std::runtime_error("Disable ASI auto exposure before timed True North capture");
    return value / 1000000.;
}
void Camera::expose(bool dark) {
    p->ready();
    if (p->exposing) throw std::logic_error("Still exposure already active");
    if (p->video) stream(false);
    p->exposureSeconds = fixedExposureSeconds();
    p->exposureStart = std::chrono::system_clock::now();
    check(ASIStartExposure(p->id, dark ? ASI_TRUE : ASI_FALSE), "start exposure");
    p->exposing = true;
}
bool Camera::exposureReady() {
    p->ready();
    if (!p->exposing) return false;
    ASI_EXPOSURE_STATUS s;
    check(ASIGetExpStatus(p->id, &s), "exposure status");
    if (s == ASI_EXP_FAILED) {
        p->exposing = false;
        throw std::runtime_error("ZWO exposure failed");
    }
    return s == ASI_EXP_SUCCESS;
}
Frame Camera::download() {
    if (!exposureReady()) throw std::logic_error("ZWO exposure not ready");
    auto f = p->buffer();
    check(ASIGetDataAfterExp(p->id, f.bytes.data(), long(f.bytes.size())), "download exposure");
    p->exposing = false;
    return f;
}
void Camera::abort() {
    p->ready();
    if (p->exposing) {
        check(ASIStopExposure(p->id), "stop exposure");
        p->exposing = false;
    }
}
void Camera::saveFits(const std::filesystem::path& file, const Frame& frame) {
    if (frame.format != ASI_IMG_RAW16) throw std::invalid_argument("RAW16 is required for sensor FITS");
    auto d = device();
    auto quoted = [](std::string value) {
        value.erase(std::remove(value.begin(), value.end(), '\''), value.end());
        return "'" + value + "'";
    };
    auto midpoint = p->exposureStart + std::chrono::duration_cast<std::chrono::system_clock::duration>(
                                           std::chrono::duration<double>(p->exposureSeconds * .5));
    auto stamp = std::chrono::system_clock::to_time_t(midpoint);
    std::tm utc{};
    gmtime_s(&utc, &stamp);
    std::ostringstream date;
    date << std::put_time(&utc, "%Y-%m-%dT%H:%M:%S") << 'Z';
    long gain = 0, flip = 0;
    for (auto& c : controls())
        if (c.id == int(ASI_GAIN))
            gain = c.value;
        else if (c.id == int(ASI_FLIP))
            flip = c.value;
    astro::fits::writeRaw16(file, frame.width, frame.height, frame.bytes,
                            {{"INSTRUME", quoted(d.name)},
                             {"GAIN", std::to_string(gain)},
                             {"EXPTIME", std::to_string(p->exposureSeconds)},
                             {"DATE-AVG", quoted(date.str())},
                             {"TIMESYS", quoted("UTC")},
                             {"FLIP", std::to_string(flip)},
                             {"BAYERPAT", quoted(d.color ? d.bayer : "MONO")},
                             {"XBINNING", std::to_string(p->bin)},
                             {"YBINNING", std::to_string(p->bin)}});
}
}  // namespace astro::zwo
