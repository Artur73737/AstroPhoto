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
    sameLineIfFits("Focus peaking");ImGui::Checkbox("Focus peaking",&config_.focusPeaking);
    help(tr("Bordi fucsia sui dettagli ad alto contrasto. Non garantisce il fuoco perfetto: verificare con zoom 5x/10x."));
    sameLineIfFits(tr("Stelle"));ImGui::Checkbox(tr("Stelle"),&config_.starPeaking);
    help(tr("Peaking per sorgenti stellari compatte: esclude pixel isolati, scie e nuclei saturi. Attivare Focus peaking."));
    if(viewLive_) {
        sameLineIfFits(tr("Punto focus"));if(ImGui::Checkbox(tr("Punto focus"),&selectFocus_)) {focusTarget_.enabled=false;++focusTarget_.revision;}
        help(tr("Clicca un dettaglio, ruota lentamente la ghiera oltre il massimo e torna al picco. OK e' relativo al miglior valore misurato, non una garanzia assoluta."));
        if(selectFocus_) {
            sameLineIfFits("- +");ImGui::BeginGroup();ImGui::BeginDisabled(!canDriveFocus(s));
            if(ImGui::Button("-##focus-near"))send(Action::Focus,static_cast<unsigned>(3-focusStep_));
            help(tr("Verso vicino: un comando al motore dell'obiettivo"));ImGui::SameLine();
            if(ImGui::Button("+##focus-far"))send(Action::Focus,static_cast<unsigned>(3+focusStep_));
            help(tr("Verso infinito: un comando al motore dell'obiettivo"));ImGui::EndDisabled();
            ImGui::SameLine();ImGui::SetNextItemWidth(em(13));
            const char* steps[]{"1 - Minimo","2 - Medio","3 - Ampio"};
            if(ImGui::BeginCombo("##focus-step",tr(steps[focusStep_-1]))){for(int i=0;i<3;++i)if(ImGui::Selectable(tr(steps[i]),focusStep_==i+1))focusStep_=i+1;ImGui::EndCombo();}
            help(tr("Tre ampiezze Canon, non gradi ne multipli esatti. Ogni clic invia un solo movimento."));ImGui::EndGroup();
            sameLineIfFits(tr("Azzera focus"));if(ImGui::Button(tr("Azzera focus")))++focusTarget_.revision;
            ImGui::TextDisabled("%s",focusDriveReason(s));
        }
        if(config_.focusPeaking) {
            ImGui::SetNextItemWidth(em(10));ImGui::SliderFloat(tr("Soglia"),&config_.peakingThreshold,10,100,"%.0f");
            help(tr("Soglia bassa: piu bordi. Soglia alta: solo dettagli con contrasto forte."));
        }
        ImGui::BeginDisabled(!s.connected||active(s));
        if(ImGui::Button(s.liveView?tr("Ferma Live"):tr("Avvia Live")))send(Action::LiveView,!s.liveView);
        ImGui::BeginDisabled(!s.liveView);
        for(int ratio:{1,5,10}){sameLineIfFits("10x");if(ImGui::RadioButton((std::to_string(ratio)+"x").c_str(),s.frame.zoom==ratio))send(Action::Zoom,ratio);}
        help(tr("10x: ritaglio 2x del frame USB 5x, come previsto dall'SDK Canon."));
        ImGui::EndDisabled();ImGui::EndDisabled();
    } else {
        ImGui::BeginDisabled(!s.connected||active(s));if(ImGui::Button(tr("Scatta")))single(s);ImGui::EndDisabled();
        sameLineIfFits("Light");ImGui::TextDisabled("%s",frameName(config_.singleCapture.type).c_str());
        if(active(s))ImGui::TextWrapped("%s",translatedMessage(s.progress.detail).c_str());
        if(!captured.status.empty())ImGui::TextWrapped("%s",translatedMessage(captured.status).c_str());
    }
    sameLineIfFits(tr("Istogrammi"));ImGui::Checkbox(tr("Istogrammi"),&showHistogram_);
    help(tr("RGB e luminanza dell'anteprima, prima degli overlay. Scala logaritmica; per CR2 usa l'anteprima incorporata, non i dati RAW."));
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
}
}
