#include <astro/core/domain/Core.hpp>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace astro {

std::filesystem::path pathFromUtf8(const std::string& value) {
    return std::filesystem::path(std::u8string(value.begin(), value.end()));
}
std::string propertyName(Property p) {
    switch (p) {
        case Property::Gain: return "Gain";
        case Property::ExposureMicroseconds: return "ExposureUs";
        case Property::Iso:
            return "ISO";
        case Property::Aperture:
            return "Av";
        case Property::Shutter:
            return "Tv";
        case Property::ExposureMode:
            return "AE";
        case Property::WhiteBalance:
            return "WB";
        case Property::ColorTemperature:
            return "Kelvin";
        case Property::PictureStyle:
            return "PictureStyle";
        case Property::ImageQuality:
            return "ImageQuality";
    }
    throw std::invalid_argument("Unknown property");
}
std::string frameName(FrameType t) {
    switch (t) {
        case FrameType::Light:
            return "Light";
        case FrameType::Dark:
            return "Dark";
        case FrameType::Flat:
            return "Flat";
        case FrameType::Bias:
            return "Bias";
    }
    throw std::invalid_argument("Unknown frame type");
}
std::string stateName(SequenceState s) {
    switch (s) {
        case SequenceState::Idle:
            return "Pronto";
        case SequenceState::Running:
            return "In esecuzione";
        case SequenceState::Paused:
            return "In pausa";
        case SequenceState::Stopping:
            return "Arresto";
        case SequenceState::Error:
            return "Errore";
    }
    return "?";
}
static void validateComponent(const std::string& s) {
    if (s.empty() || s == "." || s == ".." || s.find_first_of("<>:\"/\\|?*") != std::string::npos || s.back() == '.' ||
        s.back() == ' ' || std::any_of(s.begin(), s.end(), [](unsigned char c) { return c < 32; }))
        throw std::invalid_argument("Nome sessione non valido");
    auto name = s.substr(0, s.find('.'));
    std::transform(name.begin(), name.end(), name.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    if (name == "CON" || name == "PRN" || name == "AUX" || name == "NUL" ||
        (name.size() == 4 && (name.starts_with("COM") || name.starts_with("LPT")) && name[3] >= '1' && name[3] <= '9'))
        throw std::invalid_argument("Nome sessione riservato da Windows");
}
void validateItem(const SequenceItem& i) {
    (void)frameName(i.type);
    if (!std::isfinite(i.intervalSeconds) || i.intervalSeconds < 0 || i.intervalSeconds > 86400)
        throw std::invalid_argument("Intervallo tra scatti non valido");
    if (i.count <= 0 || !std::isfinite(i.exposureSeconds) || i.exposureSeconds <= 0 || i.exposureSeconds > 86400 ||
        !std::isfinite(i.delaySeconds) || i.delaySeconds < 0 || i.delaySeconds > 86400)
        throw std::invalid_argument("Quantita/esposizione/ritardo non validi (durata massima 24h)");
    for (const auto& [p, v] : i.settings) {
        (void)propertyName(p);
        (void)v;
    }
}
void validate(const Config& c) {
    if (c.nativeControlUi.size() > 512) throw std::invalid_argument("Too many native control UI preferences");
    for (const auto& [key, p] : c.nativeControlUi)
        if (key.size() > 200 || p.widget < 0 || p.widget > 2 || p.exposureUnit < 0 || p.exposureUnit > 2 ||
            p.order < -10000 || p.order > 10000 || p.step < 1 || p.step > 2147483647)
            throw std::invalid_argument("Invalid native control UI preferences");
    if (c.lightTheme && c.nightTheme) throw std::invalid_argument("Choose one desktop theme: Dark, Light or Night");
    if (c.keepAliveSeconds < 1 || c.keepAliveSeconds > 30 || c.reconnectAttempts < 0 || c.reconnectAttempts > 60 ||
        c.reconnectDelayMs < 100 || c.reconnectDelayMs > 10000)
        throw std::invalid_argument("Invalid connection recovery settings");
    validateStarFocus(c.starFocus);
    validateStack(c.liveStack);
    if (c.saveDestination != SaveDestination::PcOnly && c.saveDestination != SaveDestination::PcAndCamera)
        throw std::invalid_argument("Invalid save destination");
    if (c.crosshairShape != "cross" && c.crosshairShape != "gap_cross" && c.crosshairShape != "circle" &&
        c.crosshairShape != "target" && c.crosshairShape != "thirds")
        throw std::invalid_argument("Invalid crosshair shape");
    if (c.language != "it" && c.language != "en")
        throw std::invalid_argument("Lingua non supportata / Unsupported language");
    if (!c.skippedUpdateVersion.empty()) {
        const auto& version = c.skippedUpdateVersion;
        const auto first = version.find('.');
        const auto second = first == std::string::npos ? std::string::npos : version.find('.', first + 1);
        const bool valid = version.size() <= 32 && first != std::string::npos && second != std::string::npos &&
                           version.find('.', second + 1) == std::string::npos && first > 0 && second > first + 1 &&
                           second + 1 < version.size() &&
                           std::all_of(version.begin(), version.end(), [](char character) {
                               return std::isdigit(static_cast<unsigned char>(character)) || character == '.';
                           });
        if (!valid) throw std::invalid_argument("Invalid skipped update version");
    }
    if (!std::isfinite(c.peakingThreshold) || c.peakingThreshold < 10 || c.peakingThreshold > 100)
        throw std::invalid_argument("Soglia peaking fuori intervallo / Peaking threshold out of range");
    if (!std::isfinite(c.uiScale) || c.uiScale < 0.75f || c.uiScale > 2.0f)
        throw std::invalid_argument("Scala UI fuori intervallo (0.75 - 2.0)");
    validateComponent(c.sessionName);
    if (c.outputDirectory.empty() || c.tickMs < 1 || c.liveViewMs < 1 || c.liveAnalysisMs < 1 ||
        c.liveAnalysisMs > 10000 || c.lensPollMs < 100 || c.lensPollMs > 10000 || c.downloadTimeoutSeconds < 1 ||
        c.busyRetries < 0 || c.busyRetries > 100 || c.retryDelayMs < 1 || c.maxLogEntries < 1 || c.windowWidth < 640 ||
        c.windowHeight < 480)
        throw std::invalid_argument("Configurazione fuori intervallo");
    for (const auto& i : c.sequence)
        validateItem(i);
    validateItem(c.singleCapture);
}

}
