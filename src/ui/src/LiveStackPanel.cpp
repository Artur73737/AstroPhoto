#include <astro/MainWindow.hpp>
#include "Widgets.hpp"
#include <algorithm>
#include <cstdio>
namespace astro::ui {
void MainWindow::stackControls(const Snapshot& s){
    if(s.stack.preview&&s.stack.preview->revision!=stretchRevision_&&!ImGui::IsAnyItemActive()){stackDisplay_=s.stack.preview->display;stretchRevision_=s.stack.preview->revision;}
    const auto& st=s.stack;auto& o=config_.liveStack;const bool english=config_.language=="en";
    auto label=[&](const char* en,const char* it){return english?en:it;};
    ImGui::BeginDisabled(!s.connected||widgets::active(s));
    if(ImGui::Button(label("Start stack","Avvia stack"))){Command c;c.action=Action::StartStack;c.config=controlConfig();mailbox_.send(std::move(c));}
    ImGui::EndDisabled();ImGui::SameLine();ImGui::BeginDisabled(!st.active||st.stopping);
    if(ImGui::Button("Stop"))send(Action::StopStack);ImGui::EndDisabled();ImGui::SameLine();ImGui::TextUnformatted(st.phase.c_str());
    ImGui::BeginDisabled(st.active);
    bool infinite=o.frames==0;if(ImGui::Checkbox(label("Until stopped","Fino a Stop"),&infinite))o.frames=infinite?0:50;
    if(!infinite){ImGui::SetNextItemWidth(100);const auto value=std::to_string(o.frames);if(ImGui::BeginCombo("Quick settings",value.c_str())){for(int n:{50,75,100,200,300,500,700,1000})if(ImGui::Selectable(std::to_string(n).c_str(),n==o.frames))o.frames=n;ImGui::EndCombo();}ImGui::SetNextItemWidth(100);ImGui::InputInt(label("Frames (custom)","Scatti (personalizzato)"),&o.frames);o.frames=std::clamp(o.frames,1,100000);}
    if(ImGui::CollapsingHeader(label("Stack settings","Impostazioni stack"))){
        ImGui::Checkbox(label("Align stars","Allinea stelle"),&o.alignment);
        if(ImGui::IsItemHovered())ImGui::SetTooltip("%s",label("Requires a star field. Disable for a fixed indoor scene.","Richiede un campo stellare. Disattiva per una scena interna fissa."));
        ImGui::Checkbox(label("Correct isolated hot pixels","Correggi pixel caldi isolati"),&o.cosmeticCorrection);
        ImGui::Checkbox(label("Horizontal banding reduction","Riduzione banding orizzontale"),&o.horizontalBanding);
        if(o.horizontalBanding){
            float strength=float(o.bandingStrength);ImGui::SetNextItemWidth(100);if(ImGui::SliderFloat(label("Banding strength","Intensita banding"),&strength,0,1,"%.2f"))o.bandingStrength=strength;
            ImGui::SetNextItemWidth(100);ImGui::SliderInt(label("Band radius (rows)","Raggio bande (righe)"),&o.bandingRadius,2,256);
            ImGui::SetNextItemWidth(100);ImGui::InputDouble(label("Max correction","Correzione massima"),&o.bandingMaxOffset,.001,0,"%.3f");o.bandingMaxOffset=std::clamp(o.bandingMaxOffset,.0001,.1);
            ImGui::TextWrapped("%s",label("Applied to each new RAW before alignment. Strong correction can change real horizontal structures.","Applicata a ogni nuovo RAW prima dell'allineamento. Una correzione forte puo alterare strutture orizzontali reali."));
        }
        ImGui::SetNextItemWidth(100);ImGui::InputDouble(label("Delay (s, min 2)","Intervallo (s, min 2)"),&o.delaySeconds,.5,0,"%.1f");o.delaySeconds=std::clamp(o.delaySeconds,2.,3600.);
        ImGui::SetNextItemWidth(100);ImGui::InputDouble("Detection SNR",&o.detectionSnr,.5,0,"%.1f");o.detectionSnr=std::clamp(o.detectionSnr,3.,30.);
        ImGui::SetNextItemWidth(100);ImGui::InputInt(label("Minimum stars","Minimo stelle"),&o.minimumStars);o.minimumStars=std::clamp(o.minimumStars,8,100);
        ImGui::SetNextItemWidth(100);ImGui::InputDouble("Max RMS (px)",&o.maximumRms,.1,0,"%.2f");o.maximumRms=std::clamp(o.maximumRms,.1,2.);
        ImGui::Checkbox(label("Sigma rejection","Reiezione sigma"),&o.rejection);ImGui::SetNextItemWidth(100);ImGui::InputDouble("Sigma",&o.sigma,.5,0,"%.1f");o.sigma=std::clamp(o.sigma,3.,10.);
        ImGui::SetNextItemWidth(100);ImGui::InputInt("Memory (MiB)",&o.memoryLimitMb,128);o.memoryLimitMb=std::clamp(o.memoryLimitMb,512,8192);
        ImGui::TextWrapped("%s",label("Uses current Camera settings. RAW, M and fixed ISO required. Stop finishes the current exposure.","Usa i parametri attuali della fotocamera. Richiede RAW, M e ISO fisso. Stop completa lo scatto in corso."));
    }
    ImGui::EndDisabled();
    if(st.active&&st.exposureSeconds>.25){char text[80];std::snprintf(text,sizeof(text),"%.1f / %.1f s",st.exposureElapsed,st.exposureSeconds);ImGui::ProgressBar(st.phase=="exposing"?float(st.exposureElapsed/st.exposureSeconds):0,{-1,0},text);}
    ImGui::TextWrapped("%s",translatedMessage(st.detail).c_str());
    if(!st.finalFile.empty())ImGui::TextWrapped("%s",st.finalFile.c_str());
}
void MainWindow::stackLevels(const Snapshot& s){
    const auto& st=s.stack;const bool english=config_.language=="en";
    auto label=[&](const char* en,const char* it){return english?en:it;};
    const float width=std::min(ImGui::GetContentRegionAvail().x,260.f*ImGui::GetFontSize()/13.f);ImGui::SetCursorPosX(ImGui::GetCursorPosX()+std::max(0.f,ImGui::GetContentRegionAvail().x-width));
    ImGui::BeginGroup();if(st.preview)ImGui::PlotHistogram("##stackhist",st.preview->histogram.data(),256,0,"Y (linear / log count)",0,FLT_MAX,{width,ImGui::GetFontSize()*3});
    bool changed=false;ImGui::SetNextItemWidth(width*.75f);changed|=ImGui::SliderFloat("Low",&stackDisplay_.low,0,std::max(0.f,stackDisplay_.high-.0001f),"%.4f");
    ImGui::SetNextItemWidth(width*.75f);changed|=ImGui::SliderFloat("Mid",&stackDisplay_.mid,.002f,.998f,"%.3f",ImGuiSliderFlags_Logarithmic);
    ImGui::SetNextItemWidth(width*.75f);changed|=ImGui::SliderFloat("High",&stackDisplay_.high,stackDisplay_.low+.0001f,1,"%.4f",ImGuiSliderFlags_Logarithmic);
    if(ImGui::SmallButton(label("Reset display","Ripristina livelli"))){stackDisplay_={};changed=true;}
    if(changed){Command c;c.action=Action::StackStretch;c.pointU=stackDisplay_.low;c.pointV=stackDisplay_.mid;c.value=std::uint32_t(stackDisplay_.high*1000000);mailbox_.send(c);}ImGui::EndGroup();
}
}
