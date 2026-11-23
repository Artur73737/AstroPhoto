#include <astro/ui/workspace/MainWindow.hpp>
#include <astro/ui/widgets/forms/Widgets.hpp>
#include <cstdio>
namespace astro::ui {
using namespace widgets;
void MainWindow::captureControls(const Snapshot& s) {
    if (!s.connected) {
        zwoCaptureDirty_ = selectRoi_ = roiDragging_ = false;
        captureCommand_ = 0;
    }
    if (!s.capture) return;
    if (captureCommand_) {
        if (auto result = mailbox_.result(captureCommand_); result && result->finished) {
            if (!result->error.empty()) error_ = result->error;
            captureCommand_ = 0;
            zwoCaptureDirty_ = false;
        }
    }
    const bool en = config_.language == "en";
    const auto& cap = *s.capture;
    if (!zwoCaptureDirty_) {
        zwoCaptureDraft_ = cap.value;
        config_.zwo = cap.value;
    }
    if (!section("capture_area", en ? "Capture format and area" : "Formato e area di cattura")) return;
    ImGui::BeginDisabled(!s.connected || active(s) || captureCommand_ != 0);
    if (form("capture-area", 8)) {
        field(en ? "Pixel format" : "Formato pixel");
        const char* selected = "--";
        for (const auto& choice : cap.pixelFormats)
            if (choice.value == unsigned(zwoCaptureDraft_.pixelFormat)) selected = choice.label.c_str();
        if (ImGui::BeginCombo("##pixel", selected)) {
            for (const auto& choice : cap.pixelFormats)
                if (ImGui::Selectable(choice.label.c_str(), choice.value == unsigned(zwoCaptureDraft_.pixelFormat))) {
                    zwoCaptureDraft_.pixelFormat = int(choice.value);
                    zwoCaptureDirty_ = true;
                }
            ImGui::EndCombo();
        }
        field("Binning");
        if (ImGui::BeginCombo("##bin", std::to_string(zwoCaptureDraft_.bin).c_str())) {
            for (int bin : cap.bins)
                if (ImGui::Selectable(std::to_string(bin).c_str(), bin == zwoCaptureDraft_.bin)) {
                    zwoCaptureDraft_.bin = bin;
                    zwoCaptureDraft_.width = cap.sensorWidth / bin / 8 * 8;
                    zwoCaptureDraft_.height = cap.sensorHeight / bin / 2 * 2;
                    zwoCaptureDraft_.x = zwoCaptureDraft_.y = 0;
                    zwoCaptureDirty_ = true;
                }
            ImGui::EndCombo();
        }
        field(en ? "Capture area" : "Area cattura");
        char area[50];
        std::snprintf(area, sizeof(area), "%d x %d", zwoCaptureDraft_.width, zwoCaptureDraft_.height);
        if (ImGui::BeginCombo("##area", area)) {
            for (double ratio : {1., .75, 2. / 3, .5, 1. / 3, .25}) {
                int w = int(cap.sensorWidth / zwoCaptureDraft_.bin * ratio) / 8 * 8,
                    h = int(cap.sensorHeight / zwoCaptureDraft_.bin * ratio) / 2 * 2;
                if (w < 8 || h < 2) continue;
                char label[50];
                std::snprintf(label, sizeof(label), "%d x %d", w, h);
                if (ImGui::Selectable(label, w == zwoCaptureDraft_.width && h == zwoCaptureDraft_.height)) {
                    zwoCaptureDraft_.width = w;
                    zwoCaptureDraft_.height = h;
                    zwoCaptureDraft_.x = (cap.sensorWidth / zwoCaptureDraft_.bin - w) / 2;
                    zwoCaptureDraft_.y = (cap.sensorHeight / zwoCaptureDraft_.bin - h) / 2;
                    zwoCaptureDirty_ = true;
                }
            }
            ImGui::EndCombo();
        }
        ImGui::EndTable();
    }
    if (ImGui::TreeNode(en ? "Custom ROI" : "ROI personalizzata")) {
        if (form("custom-roi", 8)) {
            field("Width");
            zwoCaptureDirty_ |= ImGui::InputInt("##width", &zwoCaptureDraft_.width, 8);
            field("Height");
            zwoCaptureDirty_ |= ImGui::InputInt("##height", &zwoCaptureDraft_.height, 2);
            field("X");
            zwoCaptureDirty_ |= ImGui::InputInt("##x", &zwoCaptureDraft_.x);
            field("Y");
            zwoCaptureDirty_ |= ImGui::InputInt("##y", &zwoCaptureDraft_.y);
            ImGui::EndTable();
        }
        ImGui::TreePop();
    }
    if (ImGui::Button(en ? "Center ROI" : "Centra ROI")) {
        zwoCaptureDraft_.x = std::max(0, (cap.sensorWidth / zwoCaptureDraft_.bin - zwoCaptureDraft_.width) / 2);
        zwoCaptureDraft_.y = std::max(0, (cap.sensorHeight / zwoCaptureDraft_.bin - zwoCaptureDraft_.height) / 2);
        zwoCaptureDirty_ = true;
    }
    sameLineIfFits("Select ROI");
    if (ImGui::Checkbox("Select ROI", &selectRoi_)) {
        selectFocus_ = autoFocusPoint_ = false;
    }
    help(en ? "Drag in Live View, then Apply area. Middle mouse pans the preview."
            : "Trascina sul Live View, poi applica. Tasto centrale: sposta anteprima.");
    zwoCaptureDirty_ |= ImGui::Checkbox(en ? "Debayer preview" : "Debayer anteprima", &zwoCaptureDraft_.debayerPreview);
    if (form("stream-limit", 8)) {
        field(en ? "Preview rate" : "Frequenza preview");
        char label[50];
        if (zwoCaptureDraft_.previewFps == 0)
            std::snprintf(label, sizeof(label), "Maximum");
        else
            std::snprintf(label, sizeof(label), "%.3g fps", zwoCaptureDraft_.previewFps);
        if (ImGui::BeginCombo("##rate", label)) {
            for (double fps : {0., 480., 240., 120., 60., 30., 15., 8., 4., 2., 1., .5, .25, .125, 1. / 60}) {
                char name[50];
                if (fps == 0)
                    std::snprintf(name, sizeof(name), "Maximum");
                else if (fps >= 1)
                    std::snprintf(name, sizeof(name), "%.3g fps", fps);
                else
                    std::snprintf(name, sizeof(name), "1 frame / %.0f s", 1 / fps);
                if (ImGui::Selectable(name, fps == zwoCaptureDraft_.previewFps)) {
                    zwoCaptureDraft_.previewFps = fps;
                    zwoCaptureDirty_ = true;
                }
            }
            ImGui::EndCombo();
        }
        ImGui::EndTable();
    }
    help(en ? "Application preview cap only. Exposure, SDK USB bandwidth and High Speed remain independent."
            : "Solo limite preview. Esposizione, banda USB e High Speed sono indipendenti.");
    const bool valid = zwoCaptureDraft_.bin > 0 && zwoCaptureDraft_.width >= 8 && zwoCaptureDraft_.height >= 2 &&
                       zwoCaptureDraft_.width % 8 == 0 && zwoCaptureDraft_.height % 2 == 0 && zwoCaptureDraft_.x >= 0 &&
                       zwoCaptureDraft_.y >= 0 &&
                       zwoCaptureDraft_.x + zwoCaptureDraft_.width <= cap.sensorWidth / zwoCaptureDraft_.bin &&
                       zwoCaptureDraft_.y + zwoCaptureDraft_.height <= cap.sensorHeight / zwoCaptureDraft_.bin;
    ImGui::BeginDisabled(!zwoCaptureDirty_ || !valid);
    if (ImGui::Button(en ? "Apply capture area / stream" : "Applica area / stream", {-1, 0})) {
        Command command;
        command.action = Action::ConfigureCapture;
        command.config = config_;
        command.config.zwo = zwoCaptureDraft_;
        captureCommand_ = mailbox_.submit(std::move(command));
        selectRoi_ = false;
    }
    ImGui::EndDisabled();
    if (!valid)
        muted(en ? "ROI must fit sensor; width multiple of 8, height multiple of 2."
                 : "ROI nel sensore; larghezza multipla di 8, altezza di 2.");
    ImGui::EndDisabled();
}
}  // namespace astro::ui
