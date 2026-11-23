#include <astro/cameras/zwo/recording/FrameRecorder.hpp>
#include <astro/imaging/fits/FitsImage.hpp>
#include <nlohmann/json.hpp>
#include <condition_variable>
#include <thread>
#include <fstream>
#include <iomanip>
#include <array>
#include <algorithm>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif
namespace astro::zwo {
namespace {
std::string utf8(const std::filesystem::path& path) {
    auto text = path.u8string();
    return {text.begin(), text.end()};
}
void word(std::ostream& out, std::uint64_t n, int bytes) {
    for (int i = 0; i < bytes; ++i)
        out.put(char(n >> (i * 8)));
}
std::uint64_t ticks(std::chrono::system_clock::time_point t) {
    return 621355968000000000ull +
           std::chrono::duration_cast<std::chrono::duration<std::int64_t, std::ratio<1, 10000000>>>(
               t.time_since_epoch())
               .count();
}
std::string quoted(std::string value) {
    std::string out = "'";
    for (char c : value) {
        if (c == '\'') out += '\'';
        out += c;
    }
    return out + "'";
}
std::string numbered(std::uint64_t n, const std::string& format) {
    std::ostringstream s;
    s << "frame_" << std::setw(8) << std::setfill('0') << n << '.' << format;
    return s.str();
}
}  // namespace
struct FrameRecorder::Impl {
    struct Packet {
        sensor::Frame frame;
        std::chrono::system_clock::time_point time;
        FrameReadback readback;
    };
    FrameRecordingOptions options;
    std::filesystem::path directory, file, partial, timesPath;
    sensor::Frame geometry;
    std::string bayer, device, metadata;
    std::uint64_t reserve{}, queuedBytes{}, written{};
    mutable std::mutex mutex;
    std::condition_variable ready;
    std::deque<Packet> queue;
    bool stopping{};
    FrameRecordingStatus state;
    SteadyTime started{std::chrono::steady_clock::now()}, ended{};
    std::thread writer;
    std::fstream ser;
    std::ofstream timestamps;
    std::ofstream frameLog;
    Impl(FrameRecordingOptions o, std::filesystem::path d, sensor::Frame g, std::string b, std::string name,
         std::string info, std::uint64_t r)
        : options(o),
          directory(std::move(d)),
          geometry(std::move(g)),
          bayer(std::move(b)),
          device(std::move(name)),
          metadata(std::move(info)),
          reserve(r) {
        Config c;
        c.recording = o;
        validate(c);
        if (!std::filesystem::is_directory(directory) || !std::filesystem::is_empty(directory))
            throw std::runtime_error("Recording requires a new empty directory");
        if (!geometry.width || !geometry.height || geometry.format < 0 || geometry.format > 3)
            throw std::invalid_argument("Invalid recording geometry");
        file = directory / (options.format == "ser" ? "capture.ser" : "frames");
        partial = file;
        partial += ".partial";
        timesPath = directory / "timestamps.partial";
        state.active = true;
        state.frameLimit = options.frames;
        state.timeLimit = options.seconds;
        state.directory = utf8(directory);
        state.file = utf8(file);
        frameLog.open(directory / "frames.csv");
        frameLog << "frame,host_utc_100ns_since_year_1,exposure_us,gain,auto_exposure,auto_gain\n";
        if (!frameLog) throw std::runtime_error("Cannot create frame metadata log");
        manifest(false);
        if (options.format == "ser")
            openSer();
        else
            std::filesystem::create_directory(file);
        writer = std::thread([this] { run(); });
    }
    ~Impl() {
        stop();
        if (writer.joinable()) writer.join();
    }
    void openSer() {
        ser.open(partial, std::ios::binary | std::ios::in | std::ios::out | std::ios::trunc);
        timestamps.open(timesPath, std::ios::binary | std::ios::trunc);
        if (!ser || !timestamps) throw std::runtime_error("Cannot create recording files");
        int color = geometry.format == sensor::Bgr24 ? 101 : 0;
        if (geometry.format != sensor::Bgr24 && geometry.format != sensor::Mono8 && !bayer.empty()) {
            const std::array<std::string, 4> patterns{"RGGB", "GRBG", "GBRG", "BGGR"};
            const auto found = std::find(patterns.begin(), patterns.end(), bayer);
            if (found == patterns.end()) throw std::invalid_argument("Invalid recording CFA");
            color = 8 + int(found - patterns.begin());
        }
        ser.write("LUCAM-RECORDER", 14);
        word(ser, 0, 4);
        word(ser, color, 4);
        word(ser, 0, 4);
        word(ser, geometry.width, 4);
        word(ser, geometry.height, 4);
        word(ser, geometry.format == sensor::Raw16 ? 16 : 8, 4);
        word(ser, 0, 4);
        std::string empty(40, 0), name = device.substr(0, 40);
        for (auto& c : name)
            if (static_cast<unsigned char>(c) < 32 || static_cast<unsigned char>(c) > 126) c = '?';
        name.resize(40, 0);
        ser.write(empty.data(), 40);
        ser.write(name.data(), 40);
        ser.write(empty.data(), 40);
        word(ser, 0, 8);
        word(ser, 0, 8);
        ser.flush();
        if (!ser) throw std::runtime_error("SER header write failed");
    }
    void checkpointSer() {
        const auto end = ser.tellp();
        ser.seekp(38);
        word(ser, written, 4);
        ser.seekp(end);
        ser.flush();
        if (!ser) throw std::runtime_error("SER frame count update failed");
    }
    FrameRecordingStatus status() const {
        std::lock_guard lock(mutex);
        auto s = state;
        s.elapsed =
            std::chrono::duration<double>((ended == SteadyTime{} ? std::chrono::steady_clock::now() : ended) - started)
                .count();
        s.fps = s.elapsed > 0 ? s.written / s.elapsed : 0;
        return s;
    }
    void manifest(bool final) {
        const auto s = status();
        nlohmann::json json;
        json["schema_version"] = 1;
        json["camera"] = device;
        json["format"] = options.format;
        json["frame_type"] = frameName(options.type);
        json["limits"] = {{"frames", options.frames}, {"seconds", options.seconds}};
        json["sensor"] = {{"width", geometry.width},
                          {"height", geometry.height},
                          {"pixel_format", geometry.format},
                          {"bayer", bayer}};
        json["camera_settings"] = nlohmann::json::parse(metadata);
        json["frames_received"] = s.received;
        json["frames_written"] = s.written;
        json["queue_dropped"] = s.queueDropped;
        json["sdk_dropped"] = s.sdkDropped;
        json["elapsed_seconds"] = s.elapsed;
        json["error"] = s.error;
        json["complete"] = final && s.error.empty();
        json["timestamp_basis"] = "Host UTC at successful ASIGetVideoData read; not a hardware exposure timestamp";
        auto path = directory / "capture.json";
        auto temp = directory / "capture.json.tmp";
        {
            std::ofstream out(temp);
            out << json.dump(2);
            out.flush();
            if (!out) throw std::runtime_error("Capture manifest write failed");
        }
        // Publish the complete new checkpoint atomically.
#ifdef _WIN32
        if (!MoveFileExW(temp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            throw std::runtime_error("Capture checkpoint replacement failed: " + std::to_string(GetLastError()));
#else
        std::filesystem::rename(temp, path);
#endif
    }
    void fits(const Packet& packet) {
        const auto& f = packet.frame;
        std::vector<std::uint16_t> pixels;
        const int channels = f.format == sensor::Bgr24 ? 3 : 1;
        pixels.resize(std::size_t(f.width) * f.height * channels);
        for (std::size_t i = 0; i < pixels.size(); ++i) {
            if (f.format == sensor::Raw16)
                pixels[i] = unsigned(f.bytes[i * 2]) | (unsigned(f.bytes[i * 2 + 1]) << 8);
            else if (channels == 3)
                pixels[i] = f.bytes[(i / 3) * 3 + 2 - i % 3] * 257;
            else
                pixels[i] = f.bytes[i] * 257;
        }
        const auto path = file / numbered(written + 1, "fits");
        auto tmp = path;
        tmp += ".partial";
        std::vector<std::uint8_t> bytes(pixels.size() * 2);
        for (std::size_t i = 0; i < pixels.size(); ++i) {
            bytes[i * 2] = std::uint8_t(pixels[i]);
            bytes[i * 2 + 1] = std::uint8_t(pixels[i] >> 8);
        }
        astro::fits::writeRaw16(
            tmp, f.width, f.height, bytes,
            {{"INSTRUME", quoted(device)},
             {"BAYERPAT", quoted(channels == 1 && f.format != sensor::Mono8 && !bayer.empty() ? bayer : "MONO")},
             {"IMAGETYP", quoted(frameName(options.type))},
             {"GAIN", std::to_string(packet.readback.gain)},
             {"EXPTIME", std::to_string(packet.readback.exposureUs / 1000000.)},
             {"EXPAUTO", packet.readback.autoExposure ? "T" : "F"},
             {"GNAUTO", packet.readback.autoGain ? "T" : "F"},
             {"SRCBITS", f.format == sensor::Raw16 ? "16" : "8"}},
            channels);
        std::filesystem::rename(tmp, path);
    }
    void save(const Packet& packet) {
        const auto& f = packet.frame;
        sensor::validate(f);
        if (f.width != geometry.width || f.height != geometry.height || f.format != geometry.format)
            throw std::runtime_error("Native geometry changed during recording");
        const auto available = std::filesystem::space(directory).available;
        if (available < reserve || available - reserve < f.bytes.size() * 2ull)
            throw std::runtime_error("Recording stopped: insufficient free disk space");
        if (options.format == "ser") {
            if (!written) {
                const auto end = ser.tellp();
                ser.seekp(162);
                word(ser, 0, 8);  // Local-time timestamp unspecified; UTC follows.
                word(ser, ticks(packet.time), 8);
                ser.seekp(end);
            }
            ser.write(reinterpret_cast<const char*>(f.bytes.data()), f.bytes.size());
            word(timestamps, ticks(packet.time), 8);
            if (!ser || !timestamps) throw std::runtime_error("SER frame write failed");
        } else if (options.format == "fits")
            fits(packet);
        else
            sensor::save(file / numbered(written + 1, options.format), f, bayer, device, options.format);
        frameLog << (written + 1) << ',' << ticks(packet.time) << ',' << packet.readback.exposureUs << ','
                 << packet.readback.gain << ',' << packet.readback.autoExposure << ',' << packet.readback.autoGain
                 << '\n';
        if (!frameLog) throw std::runtime_error("Frame metadata write failed");
        ++written;
        if (options.format == "ser") checkpointSer();
        {
            std::lock_guard lock(mutex);
            state.written = written;
        }
        if (written % 25 == 0) manifest(false);
    }
    void run() {
        try {
            for (;;) {
                Packet packet;
                {
                    std::unique_lock lock(mutex);
                    ready.wait(lock, [&] { return stopping || !queue.empty(); });
                    if (queue.empty() && stopping) break;
                    packet = std::move(queue.front());
                    queuedBytes -= packet.frame.bytes.size();
                    queue.pop_front();
                }
                save(packet);
            }
            if (options.format == "ser") {
                timestamps.flush();
                timestamps.close();
                std::ifstream times(timesPath, std::ios::binary);
                if (written) ser << times.rdbuf();
                ser.flush();
                if (!ser || !times) throw std::runtime_error("SER timestamp trailer write failed");
                ser.close();
                times.close();
                std::filesystem::rename(partial, file);
                std::filesystem::remove(timesPath);
            }
            frameLog.flush();
            if (!frameLog) throw std::runtime_error("Frame metadata flush failed");
            frameLog.close();
            manifest(true);
        } catch (const std::exception& e) {
            std::lock_guard lock(mutex);
            state.error = e.what();
        }
        {
            std::lock_guard lock(mutex);
            state.active = false;
            state.finishing = false;
            state.complete = state.error.empty();
            ended = std::chrono::steady_clock::now();
            queue.clear();
            queuedBytes = 0;
        }
        try {
            manifest(true);
        } catch (const std::exception& e) {
            std::lock_guard lock(mutex);
            state.complete = false;
            state.error = e.what();
        }
    }
    void stop() {
        {
            std::lock_guard lock(mutex);
            stopping = true;
            state.finishing = state.active;
        }
        ready.notify_one();
    }
    bool push(const sensor::Frame& frame, std::chrono::system_clock::time_point time, FrameReadback readback) {
        std::lock_guard lock(mutex);
        if (stopping || !state.active) return false;
        if (options.seconds > 0 &&
            std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count() >= options.seconds) {
            stopping = true;
            state.finishing = true;
            ready.notify_one();
            return false;
        }
        if (frame.bytes.size() > std::uint64_t(options.memoryMiB) * 1024 * 1024 - queuedBytes) {
            ++state.queueDropped;
            state.error = "Recording queue full; capture stopped without silently skipping frames";
            stopping = true;
            state.finishing = true;
            ready.notify_one();
            return false;
        }
        try {
            queue.push_back({frame, time, readback});
        } catch (const std::exception& e) {
            state.error = e.what();
            stopping = true;
            state.finishing = true;
            ready.notify_one();
            return false;
        }
        queuedBytes += frame.bytes.size();
        ++state.received;
        if (state.received >= UINT32_MAX || (options.frames && state.received >= options.frames)) {
            stopping = true;
            state.finishing = true;
        }
        ready.notify_one();
        return true;
    }
};
FrameRecorder::FrameRecorder(FrameRecordingOptions o, std::filesystem::path d, sensor::Frame g, std::string b,
                             std::string name, std::string metadata, std::uint64_t reserve)
    : impl_(std::make_unique<Impl>(o, std::move(d), std::move(g), std::move(b), std::move(name), std::move(metadata),
                                   reserve)) {}
FrameRecorder::~FrameRecorder() = default;
bool FrameRecorder::push(const sensor::Frame& f, std::chrono::system_clock::time_point t, FrameReadback readback) {
    return impl_->push(f, t, readback);
}
void FrameRecorder::stop() {
    impl_->stop();
}
void FrameRecorder::finish() {
    impl_->stop();
    if (impl_->writer.joinable()) impl_->writer.join();
}
void FrameRecorder::fail(const std::string& error) {
    {
        std::lock_guard lock(impl_->mutex);
        impl_->state.error = error;
        impl_->stopping = true;
        impl_->state.finishing = impl_->state.active;
    }
    impl_->ready.notify_one();
}
void FrameRecorder::dropped(std::uint64_t n) {
    std::lock_guard lock(impl_->mutex);
    impl_->state.sdkDropped = n;
}
FrameRecordingStatus FrameRecorder::status() const {
    return impl_->status();
}
}  // namespace astro::zwo
