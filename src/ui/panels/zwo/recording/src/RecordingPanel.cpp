#include <astro/ui/workspace/MainWindow.hpp>
#include <astro/ui/widgets/forms/Widgets.hpp>
#include <limits>
namespace astro::ui {
using namespace widgets;
void MainWindow::startRecording() {
    try {
        config_.outputDirectory = pathFromUtf8(output_);
        validate(config_);
        viewLive_ = true;
        viewStack_ = viewZwo_ = false;
        Command c;
        c.action = Action::StartFrameRecording;
        c.config = config_;
        recordingCommand_ = mailbox_.submit(std::move(c));
        error_.clear();
    } catch (const std::exception& e) {
        error_ = e.what();
    }
}
void MainWindow::zwoRecording(const Snapshot& s) {
    const bool en = config_.language == "en";
    auto& options = config_.recording;
    const auto& status = s.recording;
    if (recordingCommand_) {
        if (auto result = mailbox_.result(recordingCommand_); result && result->finished) {
            if (!result->error.empty()) error_ = result->error;
            recordingCommand_ = 0;
        }
    }
    ImGui::TextWrapped(
        "%s", en ? "Records the native frames used by Live View. Camera settings are taken from the Camera tab."
                 : "Registra gli stessi frame nativi del Live View. I parametri si impostano nella scheda Camera.");
    ImGui::BeginDisabled(!s.connected || active(s) || recordingCommand_ != 0);
    if (ImGui::Button(en ? "Start capture" : "Avvia acquisizione")) startRecording();
    sameLineIfFits(en ? "Quick Capture" : "Acquisizione rapida");
    ImGui::SetNextItemWidth(-1);
    if (ImGui::BeginCombo("##quick", en ? "Quick Capture" : "Acquisizione rapida", ImGuiComboFlags_HeightLarge)) {
        if (ImGui::Selectable(en ? "Unlimited" : "Fino allo stop")) {
            options.frames = 0;
            options.seconds = 0;
            startRecording();
        }
        for (std::uint64_t n : {100, 200, 500, 1000, 2000, 3000, 5000}) {
            const auto text = std::to_string(n) + " frames";
            if (ImGui::Selectable(text.c_str())) {
                options.frames = n;
                options.seconds = 0;
                startRecording();
            }
        }
        ImGui::Separator();
        for (double seconds : {10., 20., 30., 60., 90., 120., 180.}) {
            const auto text = std::to_string(int(seconds)) + " s";
            if (ImGui::Selectable(text.c_str())) {
                options.frames = 0;
                options.seconds = seconds;
                startRecording();
            }
        }
        ImGui::EndCombo();
    }
    ImGui::EndDisabled();
    ImGui::BeginDisabled(!status.active || status.finishing);
    if (ImGui::Button(en ? "Stop capture" : "Ferma acquisizione", {-1, 0})) send(Action::StopFrameRecording);
    ImGui::EndDisabled();
    ImGui::Separator();
    ImGui::BeginDisabled(active(s) || recordingCommand_ != 0);
    int limit = options.frames ? 1 : options.seconds > 0 ? 2 : 0;
    ImGui::SetNextItemWidth(-1);
    if (ImGui::Combo(en ? "##limit" : "##limite", &limit,
                     en ? "Until stopped\0Frame count\0Duration\0" : "Fino allo stop\0Numero frame\0Durata\0")) {
        options.frames = limit == 1 ? 1000 : 0;
        options.seconds = limit == 2 ? 30 : 0;
    }
    if (form("zwo-recording", 8)) {
        if (limit == 1) {
            field(en ? "Frames" : "Frame");
            ImGui::InputScalar("##frames", ImGuiDataType_U64, &options.frames);
        }
        if (limit == 2) {
            field(en ? "Duration (s)" : "Durata (s)");
            ImGui::InputDouble("##seconds", &options.seconds, 0, 0, "%.2f");
        }
        field(en ? "Frame type" : "Tipo frame");
        int type = int(options.type);
        if (ImGui::Combo("##type", &type, "Light\0Dark\0Flat\0Bias\0")) options.type = FrameType(type);
        field(en ? "Output file" : "File di uscita");
        if (ImGui::BeginCombo("##format", options.format.c_str())) {
            for (const char* format : {"ser", "fits", "png", "tiff"})
                if (ImGui::Selectable(format, options.format == format)) options.format = format;
            ImGui::EndCombo();
        }
        ImGui::EndTable();
    }
    help(en ? "SER: one multi-frame file. FITS/PNG/TIFF: numbered native frames. Preview overlays and zoom are not "
              "recorded. Dark/Bias are labels: cover the optical path yourself."
            : "SER: un file multi-frame. FITS/PNG/TIFF: frame nativi numerati. Overlay e zoom non vengono salvati. "
              "Dark/Bias sono etichette: copri tu l'ottica.");
    if (ImGui::TreeNode(en ? "Buffer settings" : "Impostazioni buffer")) {
        ImGui::SetNextItemWidth(-1);
        ImGui::SliderInt(en ? "##memory" : "##memoria", &options.memoryMiB, 16, 2048, "%d MiB");
        ImGui::TreePop();
    }
    ImGui::EndDisabled();
    ImGui::Separator();
    ImGui::TextUnformatted(status.finishing  ? (en ? "Finishing disk writes..." : "Completamento salvataggio...")
                           : status.active   ? (en ? "Recording" : "Registrazione attiva")
                           : status.complete ? (en ? "Capture saved" : "Acquisizione salvata")
                                             : (en ? "Ready" : "Pronta"));
    const auto frames = status.directory.empty() ? options.frames : status.frameLimit;
    const auto seconds = status.directory.empty() ? options.seconds : status.timeLimit;
    const double progress = frames ? double(status.written) / frames : seconds > 0 ? status.elapsed / seconds : 0;
    char text[96];
    if (frames)
        std::snprintf(text, sizeof(text), "%llu / %llu | %.1f s", (unsigned long long)status.written,
                      (unsigned long long)frames, status.elapsed);
    else
        std::snprintf(text, sizeof(text), "%llu frames | %.1f s", (unsigned long long)status.written, status.elapsed);
    ImGui::ProgressBar(float(std::clamp(progress, 0., 1.)), {-1, 0}, text);
    ImGui::TextWrapped("%.1f saved FPS | SDK lost: %llu | queue lost: %llu", status.fps,
                       (unsigned long long)status.sdkDropped, (unsigned long long)status.queueDropped);
    ImGui::TextWrapped("%s", en ? "Stop finishes queued writes and leaves Live View running. Camera settings remain "
                                  "locked until finalization."
                                : "Stop completa i salvataggi e lascia attivo il Live View. I parametri restano "
                                  "bloccati fino al completamento.");
    if (!status.directory.empty()) ImGui::TextWrapped("%s", status.directory.c_str());
    if (!status.error.empty()) ImGui::TextWrapped("%s", status.error.c_str());
    if (!error_.empty()) ImGui::TextWrapped("%s", error_.c_str());
}
}  // namespace astro::ui
