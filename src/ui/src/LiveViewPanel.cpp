#include <astro/MainWindow.hpp>
#include "Widgets.hpp"
#include "Reticle.hpp"
namespace astro::ui {
using namespace widgets;
void MainWindow::liveView(const Snapshot& s,Preview live,Preview captured) {
    if(s.lastCapture.revision!=seenCapture_){seenCapture_=s.lastCapture.revision;viewLive_=false;}
    if(ImGui::RadioButton(tr("Scatto corrente"),!viewLive_))viewLive_=false;
    sameLineIfFits("Live View");if(ImGui::RadioButton("Live View",viewLive_))viewLive_=true;
    sameLineIfFits("Focus peaking");ImGui::Checkbox("Focus peaking",&config_.focusPeaking);
    help(tr("Bordi fucsia sui dettagli ad alto contrasto. Non garantisce il fuoco perfetto: verificare con zoom 5x/10x."));
    if(viewLive_) {
        if(config_.focusPeaking) {
            ImGui::SetNextItemWidth(em(10));ImGui::SliderFloat(tr("Soglia"),&config_.peakingThreshold,10,100,"%.0f");
            help(tr("Soglia bassa: piu bordi. Soglia alta: solo dettagli con contrasto forte."));
        }
        ImGui::BeginDisabled(!s.connected||active(s));
        if(ImGui::Button(s.liveView?tr("Ferma Live"):tr("Avvia Live")))send(Action::LiveView,!s.liveView);
        ImGui::BeginDisabled(!s.liveView);
        for(int ratio:{1,5,10}){sameLineIfFits("10x");if(ImGui::Button((std::to_string(ratio)+"x").c_str()))send(Action::Zoom,ratio);}
        for(const auto& step:std::initializer_list<std::pair<const char*,unsigned>>{{"<<",1},{"<",2},{">",4},{">>",5}}){sameLineIfFits(step.first);if(ImGui::Button(step.first))send(Action::Focus,step.second);}
        ImGui::EndDisabled();ImGui::EndDisabled();
    } else {
        ImGui::BeginDisabled(!s.connected||active(s));if(ImGui::Button(tr("Scatta")))single(s);ImGui::EndDisabled();
        sameLineIfFits("Light");ImGui::TextDisabled("%s",frameName(config_.singleCapture.type).c_str());
        if(active(s))ImGui::TextWrapped("%s",translatedMessage(s.progress.detail).c_str());
        if(!captured.status.empty())ImGui::TextWrapped("%s",translatedMessage(captured.status).c_str());
    }
    sameLineIfFits(tr("Reticolo"));ImGui::Checkbox(tr("Reticolo"),&config_.crosshairEnabled);
    const std::pair<const char*,const char*> shapes[]{{"cross","Croce"},{"gap_cross","Croce aperta"},{"circle","Cerchio"},{"target","Bersaglio"},{"thirds","Griglia terzi"}};
    const char* selected="Croce";for(const auto& [id,title]:shapes)if(config_.crosshairShape==id)selected=title;
    sameLineIfFits(tr("Griglia terzi"));ImGui::SetNextItemWidth(em(11));
    if(ImGui::BeginCombo("##reticle-shape",tr(selected))) {
        for(const auto& [id,title]:shapes)if(ImGui::Selectable(tr(title),config_.crosshairShape==id))config_.crosshairShape=id;
        ImGui::EndCombo();
    }
    ImGui::Separator();
    const auto p=viewLive_?live:captured;
    const auto available=ImGui::GetContentRegionAvail();
    ImGui::BeginChild("Preview",{std::max(em(4),available.x),std::max(em(6),available.y)},ImGuiChildFlags_Borders,ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse);
    auto pos=ImGui::GetCursorScreenPos();auto size=ImGui::GetContentRegionAvail();
    if((!viewLive_||s.liveView)&&p.texture&&p.width>0&&p.height>0) {
        float scale=std::min(size.x/static_cast<float>(p.width),size.y/static_cast<float>(p.height));ImVec2 imageSize{p.width*scale,p.height*scale};
        ImVec2 origin{pos.x+(size.x-imageSize.x)*0.5f,pos.y+(size.y-imageSize.y)*0.5f};ImGui::SetCursorScreenPos(origin);
        ImGui::Image(static_cast<ImTextureID>(p.texture),imageSize);
        if(config_.crosshairEnabled)drawReticle(*ImGui::GetWindowDrawList(),origin,imageSize,config_.crosshairShape);
    }else {
        const char* title=viewLive_?(s.liveView?tr("In attesa del frame"):tr("Live View inattivo")):tr("Nessuno scatto visualizzato");
        const char* detail=viewLive_?tr("Avvia Live per la messa a fuoco"):tr("L'ultima foto scaricata appare qui");
        float y=pos.y+std::max(0.0f,(size.y-em(3))*0.5f);
        ImGui::SetCursorScreenPos({pos.x+std::max(0.0f,(size.x-ImGui::CalcTextSize(title).x)*0.5f),y});ImGui::TextDisabled("%s",title);
        ImGui::SetCursorScreenPos({pos.x+std::max(0.0f,(size.x-ImGui::CalcTextSize(detail).x)*0.5f),y+em(1.5f)});ImGui::TextDisabled("%s",detail);
    }ImGui::EndChild();
}
}
