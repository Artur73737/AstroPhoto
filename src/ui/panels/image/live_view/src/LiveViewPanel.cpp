#include <astro/ui/workspace/MainWindow.hpp>
#include <astro/ui/widgets/forms/Widgets.hpp>
#include <astro/ui/widgets/reticle/Reticle.hpp>
#include <astro/ui/widgets/analysis/AnalysisOverlay.hpp>
#include <cstdio>
#include <imgui_internal.h>
#ifdef ASTRO_WITH_ZWO
#include <astro/features/true_north/workspace/ZwoWorkspace.hpp>
#endif
namespace astro::ui {
using namespace widgets;
void MainWindow::liveView(const Snapshot& s, Preview live, Preview captured, Preview stack) {
#ifdef ASTRO_WITH_ZWO

    ImGui::Checkbox("True North compass", &viewZwo_);
    if (viewZwo_) {
        bindNorthCamera(mailbox_, config_);
        zwoNorthWorkspace();
        return;
    }
#endif
    if (s.lastCapture.revision != seenCapture_) {
        seenCapture_ = s.lastCapture.revision;
        captureImageView_.reset();
        viewLive_ = false;
    }
    if (ImGui::RadioButton("Live stacking", viewStack_)) {
        viewStack_ = true;
        viewLive_ = false;
    }
    sameLineIfFits(tr("Scatto corrente"));
    if (ImGui::RadioButton(tr("Scatto corrente"), !viewLive_ && !viewStack_)) {
        viewLive_ = false;
        viewStack_ = false;
    }
    sameLineIfFits("Live View");
    if (ImGui::RadioButton("Live View", viewLive_ && !viewStack_)) {
        viewLive_ = true;
        viewStack_ = false;
    }
    if (viewStack_) {
        ImGui::Separator();
        displayedFps_.reset();
        const auto available = ImGui::GetContentRegionAvail();
        const bool narrow = available.x < em(50);
        const float controlsWidth = std::min(em(25), available.x * .32f);
        const float previewHeight = narrow ? std::max(em(12), available.y * .6f) : available.y;
        auto controls = [&] {
            ImGui::BeginChild(
                "Stack controls",
                {narrow ? 0 : controlsWidth, narrow ? std::max(em(6), ImGui::GetContentRegionAvail().y) : available.y},
                ImGuiChildFlags_Borders);
            stackControls(s);
            ImGui::Separator();
            stackLevels(s);
            ImGui::EndChild();
        };
        if (!narrow) {
            controls();
            ImGui::SameLine();
        }
        ImGui::BeginChild("Preview", {0, previewHeight}, ImGuiChildFlags_Borders,
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        const auto pos = ImGui::GetCursorScreenPos(), size = ImGui::GetContentRegionAvail();
        if (s.stack.preview && stack.texture && stack.width > 0 && stack.height > 0) {
            float scale = std::min(size.x / stack.width, size.y / stack.height);
            ImVec2 imageSize{stack.width * scale, stack.height * scale};
            ImGui::SetCursorScreenPos({pos.x + (size.x - imageSize.x) / 2, pos.y + (size.y - imageSize.y) / 2});
            ImGui::Image(static_cast<ImTextureID>(stack.texture), imageSize);
        } else {
            ImGui::TextWrapped("%s", config_.language == "en"
                                         ? "The accumulated RAW image appears here after the first accepted frame."
                                         : "L'immagine RAW accumulata appare qui dopo il primo frame accettato.");
        }
        auto& draw = *ImGui::GetWindowDrawList();
        char count[150];
        if (s.stack.target)
            std::snprintf(count, sizeof(count), "%d / %d frames | %d shots\n%d rejected | %.1f s integrated",
                          s.stack.accepted, s.stack.target, s.stack.captured, s.stack.rejected,
                          s.stack.integratedSeconds);
        else
            std::snprintf(count, sizeof(count), "%d frames | %d shots | %.0f s\n%d rejected | %.1f s integrated",
                          s.stack.accepted, s.stack.captured, s.stack.elapsedSeconds, s.stack.rejected,
                          s.stack.integratedSeconds);
        const auto label = ImGui::CalcTextSize(count);
        ImVec2 at{pos.x + 6, pos.y + size.y - label.y - 6};
        draw.AddRectFilled({at.x - 4, at.y - 3}, {at.x + label.x + 4, at.y + label.y + 3}, IM_COL32(0, 0, 0, 210), 4);
        draw.AddText(at, IM_COL32_WHITE, count);
        ImGui::EndChild();
        if (narrow) controls();
        return;
    }
    if (viewLive_) {
        sameLineIfFits(tr("Avvia Live"));
        ImGui::BeginDisabled(!s.connected || active(s));
        if (ImGui::Button(s.liveView ? tr("Ferma Live") : tr("Avvia Live"))) send(Action::LiveView, !s.liveView);
        ImGui::EndDisabled();
        if (s.cameraModule.id != "zwo_asi") {
            sameLineIfFits("Turbo Live Stream");
            ImGui::BeginDisabled(active(s));
            ImGui::Checkbox("Turbo Live Stream", &config_.turboLiveStream);
            ImGui::EndDisabled();
            help(tr(
                "Massima frequenza disponibile. Qualita, peaking e istogrammi invariati; maggiore uso di rete e CPU."));
        }
    } else {
        sameLineIfFits(tr("Scatta"));
        ImGui::BeginDisabled(!s.connected || active(s));
        if (ImGui::Button(tr("Scatta"))) single(s);
        ImGui::EndDisabled();
        if (!captured.status.empty()) ImGui::TextWrapped("%s", translatedMessage(captured.status).c_str());
    }
    ImGui::Separator();
    const auto p = viewLive_ ? live : captured;
    if (!viewLive_ || !s.connected || !s.liveView || !p.texture) displayedFps_.reset();
    const double fps =
        viewLive_ && s.connected && s.liveView && p.texture ? displayedFps_.update(p.revision, ImGui::GetTime()) : 0;
    const auto available = ImGui::GetContentRegionAvail();
    const bool stacked = available.x < em(50);
    const float sidebar = std::min(em(25), std::max(em(14), available.x * .32f));
    const float previewHeight = stacked ? std::max(em(8), available.y * .55f) : std::max(em(6), available.y);
    ImGui::BeginChild(
        "Preview",
        {stacked ? 0 : std::max(em(4), available.x - sidebar - ImGui::GetStyle().ItemSpacing.x), previewHeight},
        ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    auto pos = ImGui::GetCursorScreenPos();
    auto size = ImGui::GetContentRegionAvail();
    if ((!viewLive_ || s.liveView) && p.texture && p.width > 0 && p.height > 0) {
        float scale = std::min(size.x / static_cast<float>(p.width), size.y / static_cast<float>(p.height));
        ImVec2 imageSize{p.width * scale, p.height * scale};
        ImVec2 origin{pos.x + (size.x - imageSize.x) * 0.5f, pos.y + (size.y - imageSize.y) * 0.5f};
        ImGui::SetCursorScreenPos(origin);
        auto& viewport = viewLive_ ? liveImageView_ : captureImageView_;
        if (viewLive_ &&
            (zoomImageWidth_ != p.width || zoomImageHeight_ != p.height || zoomSdkRatio_ != s.frame.zoom)) {
            viewport.reset();
            zoomImageWidth_ = p.width;
            zoomImageHeight_ = p.height;
            zoomSdkRatio_ = s.frame.zoom;
        }
        const auto mousePosition = ImGui::GetIO().MousePos;
        const bool inImage = mousePosition.x >= origin.x && mousePosition.y >= origin.y &&
                             mousePosition.x < origin.x + imageSize.x && mousePosition.y < origin.y + imageSize.y;
        if (inImage) {
            ImGui::SetKeyOwner(ImGuiKey_MouseWheelY, ImGui::GetID("image-wheel"));
            if (ImGui::GetIO().MouseWheel != 0)
                viewport.wheel(ImGui::GetIO().MouseWheel, (mousePosition.x - origin.x) / imageSize.x,
                               (mousePosition.y - origin.y) / imageSize.y);
            if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle))
                viewport.pan(ImGui::GetIO().MouseDelta.x / imageSize.x, ImGui::GetIO().MouseDelta.y / imageSize.y);
            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && !selectFocus_ && !autoFocusPoint_ && !selectRoi_)
                viewport.reset();
        }
        const ImVec2 uv0{float(viewport.left()), float(viewport.top())},
            uv1{float(viewport.left() + viewport.span()), float(viewport.top() + viewport.span())};
        ImGui::Image(static_cast<ImTextureID>(p.texture), imageSize, uv0, uv1);
        const ImVec2 fullSize{imageSize.x / float(viewport.span()), imageSize.y / float(viewport.span())};
        const ImVec2 fullOrigin{origin.x - uv0.x * fullSize.x, origin.y - uv0.y * fullSize.y};
        const auto histSize = histogramSize(imageSize);
        const ImVec2 histOrigin{origin.x + imageSize.x - histSize.x - 4, origin.y + 4};
        const auto mouse = ImGui::GetIO().MousePos;
        const bool onHistogram = showHistogram_ && mouse.x >= histOrigin.x && mouse.y >= histOrigin.y &&
                                 mouse.x <= histOrigin.x + histSize.x && mouse.y <= histOrigin.y + histSize.y;
        const double pointerU =
            std::clamp(viewport.left() + (mouse.x - origin.x) / imageSize.x * viewport.span(), 0., 1.);
        const double pointerV =
            std::clamp(viewport.top() + (mouse.y - origin.y) / imageSize.y * viewport.span(), 0., 1.);
        if (viewLive_ && selectRoi_ && s.capture && !active(s)) {
            if (inImage && !onHistogram && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                roiDragging_ = true;
                roiStartU_ = pointerU;
                roiStartV_ = pointerV;
            }
            if (roiDragging_ && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
                auto current = s.capture->value;
                zwoCaptureDraft_ = current;
                zwoCaptureDraft_.width = std::max(8, int(std::abs(pointerU - roiStartU_) * current.width) / 8 * 8);
                zwoCaptureDraft_.height = std::max(2, int(std::abs(pointerV - roiStartV_) * current.height) / 2 * 2);
                zwoCaptureDraft_.x = current.x + int(std::min(pointerU, roiStartU_) * current.width);
                zwoCaptureDraft_.y = current.y + int(std::min(pointerV, roiStartV_) * current.height);
                zwoCaptureDraft_.x = std::min(zwoCaptureDraft_.x, current.x + current.width - zwoCaptureDraft_.width);
                zwoCaptureDraft_.y = std::min(zwoCaptureDraft_.y, current.y + current.height - zwoCaptureDraft_.height);
                zwoCaptureDirty_ = true;
                roiDragging_ = false;
            }
        }
        if (viewLive_ && !selectRoi_ && (selectFocus_ || autoFocusPoint_) && !active(s) && ImGui::IsItemHovered() &&
            !onHistogram && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            focusTarget_ = {true, float(pointerU), float(pointerV), focusTarget_.revision + 1};
            if (autoFocusPoint_ && canDriveFocus(s) && s.frame.zoom == 1) {
                Command c;
                c.action = Action::StartPointFocus;
                c.pointU = focusTarget_.u;
                c.pointV = focusTarget_.v;
                mailbox_.send(std::move(c));
            }
        }
        auto& draw = *ImGui::GetWindowDrawList();
        draw.PushClipRect(origin, {origin.x + imageSize.x, origin.y + imageSize.y}, true);
        if (roiDragging_) {
            const ImVec2 start{fullOrigin.x + float(roiStartU_) * fullSize.x,
                               fullOrigin.y + float(roiStartV_) * fullSize.y};
            draw.AddRect({std::min(start.x, mouse.x), std::min(start.y, mouse.y)},
                         {std::max(start.x, mouse.x), std::max(start.y, mouse.y)}, IM_COL32(255, 215, 70, 255), 0, 0,
                         2);
        }
        if (viewport.zoom > 1) {
            char text[40];
            std::snprintf(text, sizeof(text), "%.0f%% | middle: pan", viewport.zoom * 100);
            draw.AddText({origin.x + 6, origin.y + imageSize.y - 20}, IM_COL32_WHITE, text);
        }
        if (viewLive_) {
            char text[32];
            std::snprintf(text, sizeof(text), "%.1f FPS", fps);
            const ImVec2 at{origin.x + 6, origin.y + 6}, label = ImGui::CalcTextSize(text);
            draw.AddRectFilled({at.x - 4, at.y - 2}, {at.x + label.x + 4, at.y + label.y + 2}, IM_COL32(0, 0, 0, 185),
                               3);
            draw.AddText(at, IM_COL32(235, 240, 245, 255), text);
        }
        if (config_.crosshairEnabled)
            drawReticle(*ImGui::GetWindowDrawList(), fullOrigin, fullSize, config_.crosshairShape);
        if (showHistogram_ && p.histogram) drawHistogram(draw, histOrigin, histSize, *p.histogram);
        if (viewLive_ && selectFocus_) {
            if (focusTarget_.enabled)
                drawFocusReading(draw, fullOrigin, fullSize, p.width, p.height, p.focus, focusTarget_,
                                 config_.focusPeaking);
            else
                draw.AddText({origin.x + 6, origin.y + ImGui::GetTextLineHeight() + 12}, IM_COL32(255, 205, 85, 255),
                             tr("Clicca il punto da mettere a fuoco"));
        }
        if (viewLive_ && autoFocusPoint_) {
            const auto& af = s.pointAutofocus;
            const auto color = af.phase == PointAfPhase::Complete ? IM_COL32(90, 240, 140, 255)
                               : af.phase == PointAfPhase::Failed ? IM_COL32(255, 150, 80, 255)
                                                                  : IM_COL32(255, 215, 90, 255);
            if (af.phase != PointAfPhase::Idle) {
                const ImVec2 center{fullOrigin.x + float(af.u) * fullSize.x, fullOrigin.y + float(af.v) * fullSize.y};
                draw.AddRect({center.x - fullSize.x * .1f, center.y - fullSize.y * .1f},
                             {center.x + fullSize.x * .1f, center.y + fullSize.y * .1f}, color, 3, 0, 2);
            }
            draw.AddText({origin.x + 6, origin.y + ImGui::GetTextLineHeight() + 12}, color,
                         af.detail.empty() ? tr("Tocca Live View 1x: AF della camera, OK solo dopo conferma.")
                                           : translatedMessage(af.detail).c_str());
        }
        draw.PopClipRect();
    } else {
        const char* title = viewLive_ ? (s.liveView ? tr("In attesa del frame") : tr("Live View inattivo"))
                                      : tr("Nessuno scatto visualizzato");
        const char* detail =
            viewLive_ ? tr("Avvia Live per la messa a fuoco") : tr("L'ultima foto scaricata appare qui");
        float y = pos.y + std::max(0.0f, (size.y - em(3)) * 0.5f);
        ImGui::SetCursorScreenPos({pos.x + std::max(0.0f, (size.x - ImGui::CalcTextSize(title).x) * 0.5f), y});
        ImGui::TextDisabled("%s", title);
        ImGui::SetCursorScreenPos(
            {pos.x + std::max(0.0f, (size.x - ImGui::CalcTextSize(detail).x) * 0.5f), y + em(1.5f)});
        ImGui::TextDisabled("%s", detail);
    }
    ImGui::EndChild();
    if (!stacked) ImGui::SameLine();
    if (pendingFocusScroll_ >= 0) {
        ImGui::SetNextWindowScroll({-1, pendingFocusScroll_});
        pendingFocusScroll_ = -1;
    }
    ImGui::BeginChild("Focus controls",
                      {0, stacked ? std::max(em(6), ImGui::GetContentRegionAvail().y) : std::max(em(6), available.y)},
                      ImGuiChildFlags_Borders);
    focusControls(s);
    focusScroll_ = ImGui::GetScrollY();
    ImGui::EndChild();
}
}  // namespace astro::ui
