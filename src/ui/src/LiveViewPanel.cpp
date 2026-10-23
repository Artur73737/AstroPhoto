#include <astro/MainWindow.hpp>
#include "Widgets.hpp"
#include "Reticle.hpp"
#include "AnalysisOverlay.hpp"
namespace astro::ui {
using namespace widgets;
void MainWindow::liveView(const Snapshot& s,Preview live,Preview captured) {
    if(s.lastCapture.revision!=seenCapture_){seenCapture_=s.lastCapture.revision;viewLive_=false;}
    if(ImGui::RadioButton(tr("Scatto corrente"),!viewLive_))viewLive_=false;
    sameLineIfFits("Live View");if(ImGui::RadioButton("Live View",viewLive_))viewLive_=true;
    if(viewLive_) {
        sameLineIfFits(tr("Avvia Live"));ImGui::BeginDisabled(!s.connected||active(s));
        if(ImGui::Button(s.liveView?tr("Ferma Live"):tr("Avvia Live")))send(Action::LiveView,!s.liveView);
        ImGui::EndDisabled();
    } else {
        sameLineIfFits(tr("Scatta"));ImGui::BeginDisabled(!s.connected||active(s));if(ImGui::Button(tr("Scatta")))single(s);ImGui::EndDisabled();
        if(!captured.status.empty())ImGui::TextWrapped("%s",translatedMessage(captured.status).c_str());
    }
    ImGui::Separator();
    const auto p=viewLive_?live:captured;
    const auto available=ImGui::GetContentRegionAvail();
    const float sidebar=std::min(em(25),std::max(em(14),available.x*.32f));
    ImGui::BeginChild("Preview",{std::max(em(4),available.x-sidebar-ImGui::GetStyle().ItemSpacing.x),std::max(em(6),available.y)},ImGuiChildFlags_Borders,ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse);
    auto pos=ImGui::GetCursorScreenPos();auto size=ImGui::GetContentRegionAvail();
    if((!viewLive_||s.liveView)&&p.texture&&p.width>0&&p.height>0) {
        float scale=std::min(size.x/static_cast<float>(p.width),size.y/static_cast<float>(p.height));ImVec2 imageSize{p.width*scale,p.height*scale};
        ImVec2 origin{pos.x+(size.x-imageSize.x)*0.5f,pos.y+(size.y-imageSize.y)*0.5f};ImGui::SetCursorScreenPos(origin);
        ImGui::Image(static_cast<ImTextureID>(p.texture),imageSize);
        const auto histSize=histogramSize(imageSize);
        const ImVec2 histOrigin{origin.x+imageSize.x-histSize.x-4,origin.y+4};
        const auto mouse=ImGui::GetIO().MousePos;
        const bool onHistogram=showHistogram_&&mouse.x>=histOrigin.x&&mouse.y>=histOrigin.y&&mouse.x<=histOrigin.x+histSize.x&&mouse.y<=histOrigin.y+histSize.y;
        if(viewLive_&&selectFocus_&&ImGui::IsItemHovered()&&!onHistogram&&ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            focusTarget_={true,std::clamp((mouse.x-origin.x)/imageSize.x,0.f,1.f),std::clamp((mouse.y-origin.y)/imageSize.y,0.f,1.f),focusTarget_.revision+1};
        }
        auto& draw=*ImGui::GetWindowDrawList();draw.PushClipRect(origin,{origin.x+imageSize.x,origin.y+imageSize.y},true);
        if(config_.crosshairEnabled)drawReticle(*ImGui::GetWindowDrawList(),origin,imageSize,config_.crosshairShape);
        if(showHistogram_&&p.histogram)drawHistogram(draw,histOrigin,histSize,*p.histogram);
        if(viewLive_&&selectFocus_) {
            if(focusTarget_.enabled)drawFocusReading(draw,origin,imageSize,p.width,p.height,p.focus,focusTarget_,config_.focusPeaking);
            else draw.AddText({origin.x+6,origin.y+6},IM_COL32(255,205,85,255),tr("Clicca il punto da mettere a fuoco"));
        }
        draw.PopClipRect();
    }else {
        const char* title=viewLive_?(s.liveView?tr("In attesa del frame"):tr("Live View inattivo")):tr("Nessuno scatto visualizzato");
        const char* detail=viewLive_?tr("Avvia Live per la messa a fuoco"):tr("L'ultima foto scaricata appare qui");
        float y=pos.y+std::max(0.0f,(size.y-em(3))*0.5f);
        ImGui::SetCursorScreenPos({pos.x+std::max(0.0f,(size.x-ImGui::CalcTextSize(title).x)*0.5f),y});ImGui::TextDisabled("%s",title);
        ImGui::SetCursorScreenPos({pos.x+std::max(0.0f,(size.x-ImGui::CalcTextSize(detail).x)*0.5f),y+em(1.5f)});ImGui::TextDisabled("%s",detail);
    }ImGui::EndChild();
    ImGui::SameLine();ImGui::BeginChild("Focus controls",{0,std::max(em(6),available.y)},ImGuiChildFlags_Borders);
    focusControls(s);ImGui::EndChild();
}
}
