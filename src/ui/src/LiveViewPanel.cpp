#include <astro/MainWindow.hpp>
#include "Widgets.hpp"
namespace astro::ui {
using namespace widgets;
void MainWindow::liveView(const Snapshot& s,Preview live,Preview captured) {
    if(s.lastCapture.revision!=seenCapture_){seenCapture_=s.lastCapture.revision;viewLive_=false;}
    if(ImGui::RadioButton("Scatto corrente",!viewLive_))viewLive_=false;
    sameLineIfFits("Live View");if(ImGui::RadioButton("Live View",viewLive_))viewLive_=true;
    sameLineIfFits("Focus peaking");ImGui::Checkbox("Focus peaking",&config_.focusPeaking);
    help("Bordi fucsia sui dettagli ad alto contrasto. Non garantisce il fuoco perfetto: verificare con zoom 5x/10x.");
    if(viewLive_) {
        if(config_.focusPeaking) {
            ImGui::SetNextItemWidth(em(10));ImGui::SliderFloat("Soglia",&config_.peakingThreshold,10,100,"%.0f");
            help("Soglia bassa: piu bordi. Soglia alta: solo dettagli con contrasto forte.");
        }
        ImGui::BeginDisabled(!s.connected||active(s));
        if(ImGui::Button(s.liveView?"Ferma Live":"Avvia Live"))send(Action::LiveView,!s.liveView);
        ImGui::BeginDisabled(!s.liveView);
        for(int ratio:{1,5,10}){sameLineIfFits("10x");if(ImGui::Button((std::to_string(ratio)+"x").c_str()))send(Action::Zoom,ratio);}
        for(const auto& step:std::initializer_list<std::pair<const char*,unsigned>>{{"<<",1},{"<",2},{">",4},{">>",5}}){sameLineIfFits(step.first);if(ImGui::Button(step.first))send(Action::Focus,step.second);}
        ImGui::EndDisabled();ImGui::EndDisabled();
    } else {
        ImGui::BeginDisabled(!s.connected||active(s));if(ImGui::Button("Scatta"))single(s);ImGui::EndDisabled();
        sameLineIfFits("Light");ImGui::TextDisabled("%s",frameName(config_.singleCapture.type).c_str());
        if(active(s))ImGui::TextWrapped("%s",s.progress.detail.c_str());
        if(!captured.status.empty())ImGui::TextWrapped("%s",captured.status.c_str());
    }
    sameLineIfFits("Reticolo");ImGui::Checkbox("Reticolo",&crosshair_);ImGui::Separator();
    const auto p=viewLive_?live:captured;
    const auto available=ImGui::GetContentRegionAvail();
    ImGui::BeginChild("Preview",{std::max(em(4),available.x),std::max(em(6),available.y)},ImGuiChildFlags_Borders,ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse);
    auto pos=ImGui::GetCursorScreenPos();auto size=ImGui::GetContentRegionAvail();
    if((!viewLive_||s.liveView)&&p.texture&&p.width>0&&p.height>0) {
        float scale=std::min(size.x/static_cast<float>(p.width),size.y/static_cast<float>(p.height));ImVec2 imageSize{p.width*scale,p.height*scale};
        ImVec2 origin{pos.x+(size.x-imageSize.x)*0.5f,pos.y+(size.y-imageSize.y)*0.5f};ImGui::SetCursorScreenPos(origin);
        ImGui::Image(static_cast<ImTextureID>(p.texture),imageSize);
        if(crosshair_){auto* d=ImGui::GetWindowDrawList();ImVec2 center{origin.x+imageSize.x/2,origin.y+imageSize.y/2};auto color=IM_COL32(120,190,255,170);d->AddLine({center.x,origin.y},{center.x,origin.y+imageSize.y},color);d->AddLine({origin.x,center.y},{origin.x+imageSize.x,center.y},color);}
    }else {
        const char* title=viewLive_?(s.liveView?"In attesa del frame":"Live View inattivo"):"Nessuno scatto visualizzato";
        const char* detail=viewLive_?"Avvia Live per la messa a fuoco":"L\'ultima foto scaricata appare qui";
        float y=pos.y+std::max(0.0f,(size.y-em(3))*0.5f);
        ImGui::SetCursorScreenPos({pos.x+std::max(0.0f,(size.x-ImGui::CalcTextSize(title).x)*0.5f),y});ImGui::TextDisabled("%s",title);
        ImGui::SetCursorScreenPos({pos.x+std::max(0.0f,(size.x-ImGui::CalcTextSize(detail).x)*0.5f),y+em(1.5f)});ImGui::TextDisabled("%s",detail);
    }ImGui::EndChild();
}
}
