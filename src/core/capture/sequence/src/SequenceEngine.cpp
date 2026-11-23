#include <astro/core/domain/Core.hpp>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace astro {
bool SequenceEngine::active() const {
    return progress_.state == SequenceState::Running || progress_.state == SequenceState::Paused ||
           progress_.state == SequenceState::Stopping;
}
void SequenceEngine::start(const Config& c) {
    if (active()) throw std::logic_error("Sequenza gia attiva");
    validate(c);
    if (c.sequence.empty()) throw std::invalid_argument("Sequenza vuota");
    if (!camera_.connected()) throw CameraError("Fotocamera non connessa");
    organizer_.prepare(c);
    const auto space = files_.available(organizer_.session(c));
    if (space <= c.reserveBytes) throw std::runtime_error("Spazio disco insufficiente");
    camera_.prepareHost(space - c.reserveBytes, c.saveDestination);
    config_ = c;
    progress_ = {};
    for (const auto& i : c.sequence)
        progress_.total += static_cast<std::uint64_t>(i.count);
    progress_.state = SequenceState::Running;
    progress_.detail = "Preparazione";
    shotInRow_ = 0;
    retries_ = 0;
    captureStarted_ = false;
    phase_ = Phase::Configure;
    retryAt_ = {};
    nextCaptureAt_ = clock_.steadyNow();
    log_("Info", "Sequenza avviata");
}
void SequenceEngine::startSingle(const Config& c) {
    auto single = c;
    single.sequence = {c.singleCapture};
    single.sequence[0].count = 1;
    start(single);
    log_("Info", "Scatto singolo: " + frameName(c.singleCapture.type));
}
void SequenceEngine::pause() {
    if (progress_.state == SequenceState::Running) progress_.pauseRequested = true;
}
void SequenceEngine::resume() {
    if (progress_.state == SequenceState::Paused) progress_.state = SequenceState::Running;
    progress_.pauseRequested = false;
}
void SequenceEngine::fail(const std::string& message) {
    auto detail = message;
    if (captureStarted_) {
        try {
            camera_.abortCapture();
            captureStarted_ = false;
        } catch (const std::exception& e) {
            detail += "; arresto: ";
            detail += e.what();
        }
    }
    progress_.state = SequenceState::Error;
    progress_.detail = detail;
    log_("Error", detail);
}
void SequenceEngine::stop() {
    if (!active() && !captureStarted_) return;
    progress_.state = SequenceState::Stopping;
    try {
        if (captureStarted_) camera_.abortCapture();
        captureStarted_ = false;
        progress_.state = SequenceState::Idle;
        progress_.pauseRequested = false;
        progress_.detail = "Sequenza interrotta";
        log_("Info", progress_.detail);
    } catch (const std::exception& e) {
        fail(e.what());
    }
}
void SequenceEngine::tick() {
    if (progress_.state != SequenceState::Running) return;
    try {
        if (!camera_.connected()) throw CameraError("Fotocamera disconnessa durante la sequenza");
        const auto now = clock_.steadyNow();
        if (now < retryAt_) return;
        auto& item = config_.sequence.at(progress_.row);
        if (phase_ == Phase::Configure) {
            if (progress_.pauseRequested) {
                progress_.state = SequenceState::Paused;
                return;
            }
            const auto available = files_.available(config_.outputDirectory);
            if (available <= config_.reserveBytes) throw std::runtime_error("Spazio disco insufficiente");
            for (const auto& [p, v] : item.settings)
                camera_.set(p, v);
            for (const auto& p : camera_.properties())
                if (p.id == Property::ExposureMicroseconds) {
                    if (p.currentLabel == "Auto" || p.value == 0)
                        throw CameraError("Fixed native exposure required for timed capture");
                    item.exposureSeconds = p.value / 1000000.;
                }
            deadline_ = std::max(nextCaptureAt_,
                                 now + std::chrono::duration_cast<SteadyTime::duration>(
                                           std::chrono::duration<double>(shotInRow_ == 0 ? item.delaySeconds : 0)));
            phase_ = Phase::Delay;
            progress_.detail = "Attesa prima dello scatto";
            retries_ = 0;
        }
        if (phase_ == Phase::Delay && progress_.pauseRequested) {
            progress_.state = SequenceState::Paused;
            return;
        }
        if (phase_ == Phase::Delay && now < deadline_)
            progress_.detail =
                "Prossimo scatto tra " +
                std::to_string(static_cast<int>(std::ceil(std::chrono::duration<double>(deadline_ - now).count()))) +
                " s";
        if (phase_ == Phase::Delay && now >= deadline_) phase_ = Phase::Capture;
        if (phase_ == Phase::Capture) {
            // Never automatically repeat a shutter command: completion can be ambiguous.
            captureStarted_ = true;
            camera_.beginCapture(item, organizer_.stem(config_, item, ++counter_));
            deadline_ = now + std::chrono::duration_cast<SteadyTime::duration>(
                                  std::chrono::duration<double>(item.exposureSeconds + config_.downloadTimeoutSeconds));
            phase_ = Phase::Download;
            progress_.detail = "Esposizione / download";
        }
        if (phase_ == Phase::Download) {
            auto result = camera_.pollCapture();
            if (result.complete) {
                if (result.files.empty()) throw CameraError("Trasferimento senza file");
                captureStarted_ = false;
                ++progress_.completed;
                ++shotInRow_;
                ++lastCapture_.revision;
                lastCapture_.type = item.type;
                lastCapture_.files = result.files;
                nextCaptureAt_ = now + std::chrono::duration_cast<SteadyTime::duration>(
                                           std::chrono::duration<double>(item.intervalSeconds));
                for (const auto& path : result.files)
                    log_("Info", "Salvato: " + path.string());
                if (shotInRow_ >= item.count) {
                    ++progress_.row;
                    shotInRow_ = 0;
                }
                phase_ = Phase::Configure;
                if (progress_.completed == progress_.total) {
                    progress_.state = SequenceState::Idle;
                    progress_.pauseRequested = false;
                    progress_.detail = "Sequenza completata";
                    log_("Info", progress_.detail);
                }
            } else if (now >= deadline_)
                throw CameraError("Timeout in attesa del download; verificare la fotocamera prima di riprovare");
        }
    } catch (const CameraError& e) {
        if (e.retryable && phase_ == Phase::Configure && retries_ < config_.busyRetries) {
            ++retries_;
            retryAt_ = clock_.steadyNow() +
                       std::chrono::milliseconds(static_cast<std::int64_t>(config_.retryDelayMs) * retries_);
            log_("Warning", e.what());
        } else
            fail(e.what());
    } catch (const std::exception& e) {
        fail(e.what());
    }
}

}  // namespace astro
