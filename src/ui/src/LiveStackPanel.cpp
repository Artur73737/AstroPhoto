#include <astro/MainWindow.hpp>
#include "Widgets.hpp"
#include <algorithm>
#include <cstdio>
namespace astro::ui {
void MainWindow::stackControls(const Snapshot& s){
    if(s.stack.preview&&s.stack.preview->revision!=stretchRevision_&&!ImGui::IsAnyItemActive()){stackDisplay_=s.stack.preview->display;stretchRevision_=s.stack.preview->revision;}
    const auto& st=s.stack;auto& o=config_.liveStack;const bool english=config_.language=="en";
    auto label=[&](const char* en,const char* it){return english?en:it;};
    ImGui::BeginDisabled((!s.connected&&o.sourceDirectory.empty())||widgets::active(s));
    if(ImGui::Button(label("Start stack","Avvia stack"))){Command c;c.action=Action::StartStack;c.config=controlConfig();mailbox_.send(std::move(c));}
    ImGui::EndDisabled();ImGui::SameLine();ImGui::BeginDisabled(!st.active||st.stopping);
    if(ImGui::Button("Stop"))send(Action::StopStack);ImGui::EndDisabled();ImGui::SameLine();ImGui::TextUnformatted(st.phase.c_str());
    ImGui::BeginDisabled(!st.active||st.stopping);
    if(ImGui::Button(st.pauseRequested?label("Resume","Riprendi"):label("Pause","Pausa")))send(st.pauseRequested?Action::ResumeStack:Action::PauseStack);
    ImGui::EndDisabled();
    ImGui::BeginDisabled(st.active);
    if(ImGui::TreeNode(label("Rebuild from CR2 folder","Ricostruisci da cartella CR2"))){ImGui::TextWrapped("%s",label("Empty = camera. Select the frames folder of an interrupted run to rebuild into a new session. Originals are referenced, never overwritten.","Vuoto = fotocamera. Scegli la cartella frames di una sessione interrotta per ricostruire in una nuova sessione. Gli originali restano intatti."));ImGui::SetNextItemWidth(-1);ImGui::InputText("##source",&o.sourceDirectory);ImGui::TreePop();}
    if(ImGui::BeginCombo(label("Frame type","Tipo frame"),o.frameType.c_str())){for(const auto* type:{"Light","Dark","Flat","Bias","Dark-flat"})if(ImGui::Selectable(type,o.frameType==type)){o.frameType=type;if(o.frameType!="Light"){o.alignment=false;o.drizzleScale=1;o.backgroundNormalization=o.gradientRemoval=false;o.biasDirectory.clear();o.darkDirectory.clear();o.flatDirectory.clear();o.darkFlatDirectory.clear();}}ImGui::EndCombo();}
    if(o.frameType!="Light")ImGui::TextWrapped("%s",label("Prepare the lens cap / flat illumination manually. A float Bayer master and original CR2s are saved; use the frames folder for calibration.","Prepara manualmente tappo / illuminazione flat. Salva master Bayer float e CR2 originali; usa la cartella frames per calibrare."));
    ImGui::Checkbox(label("Use homogeneous sequence plan","Usa sequenza omogenea"),&o.fromSequence);ImGui::Checkbox(label("Rebuild interrupted run on startup","Ricostruisci sessione interrotta all avvio"),&o.autoRebuild);
    bool infinite=o.frames==0;if(ImGui::Checkbox(label("Until stopped","Fino a Stop"),&infinite))o.frames=infinite?0:50;
    if(!infinite){ImGui::SetNextItemWidth(100);const auto value=std::to_string(o.frames);if(ImGui::BeginCombo("Quick settings",value.c_str())){for(int n:{50,75,100,200,300,500,700,1000})if(ImGui::Selectable(std::to_string(n).c_str(),n==o.frames))o.frames=n;ImGui::EndCombo();}ImGui::SetNextItemWidth(100);ImGui::InputInt(label("Frames (custom)","Scatti (personalizzato)"),&o.frames);o.frames=std::clamp(o.frames,1,100000);}
    if(ImGui::CollapsingHeader(label("Stack settings","Impostazioni stack"))){
        ImGui::Checkbox(label("Align stars","Allinea stelle"),&o.alignment);
        ImGui::Checkbox(label("Affine refinement","Raffinamento affine"),&o.affineAlignment);
        ImGui::SetNextItemWidth(100);if(ImGui::BeginCombo("Drizzle",std::to_string(o.drizzleScale).c_str())){for(double n:{1.,1.5,2.,3.})if(ImGui::Selectable(std::to_string(n).c_str(),n==o.drizzleScale))o.drizzleScale=n;ImGui::EndCombo();}
        if(o.drizzleScale>1){ImGui::SetNextItemWidth(100);ImGui::InputDouble("Drop fraction",&o.drizzlePixfrac,.1,0,"%.2f");o.drizzlePixfrac=std::clamp(o.drizzlePixfrac,.1,1.);ImGui::TextWrapped("%s",label("Subpixel dithers are needed for added detail. Larger scales need more memory; empty output pixels retain zero coverage.","Servono spostamenti subpixel per ottenere dettaglio. Scale maggiori richiedono piu memoria; i pixel vuoti mantengono copertura zero."));}
        ImGui::Checkbox(label("Also export float32 TIFF","Esporta anche TIFF float32"),&o.exportTiff);ImGui::Checkbox(label("Also export XISF","Esporta anche XISF"),&o.exportXisf);
        if(ImGui::TreeNode("ASTAP / plate solving")){ImGui::TextWrapped("%s",label("Optional local ASTAP executable and star database. Solves the final FITS; WCS and diagnostics are saved beside it. No mount movement.","ASTAP locale opzionale con database stellare. Risolve il FITS finale; WCS e diagnostica sono salvati accanto. Nessun movimento montatura."));ImGui::SetNextItemWidth(-1);ImGui::InputText("##solverexe",&o.solverExecutable);ImGui::SetNextItemWidth(-1);ImGui::InputText("##solverdb",&o.solverDatabase);ImGui::SetNextItemWidth(100);ImGui::InputDouble("Field height (deg, 0 auto)",&o.solverFov,.1,0,"%.2f");o.solverFov=std::clamp(o.solverFov,0.,180.);ImGui::SetNextItemWidth(100);ImGui::InputInt("Solver timeout (s)",&o.solverTimeoutSeconds);o.solverTimeoutSeconds=std::clamp(o.solverTimeoutSeconds,5,300);ImGui::TreePop();}

        ImGui::Checkbox(label("Online Winsor limits","Limiti Winsor online"),&o.winsorizedRejection);

        ImGui::Checkbox(label("Normalize background","Normalizza fondo"),&o.backgroundNormalization);
        ImGui::Checkbox(label("Remove linear gradient","Rimuovi gradiente lineare"),&o.gradientRemoval);
        ImGui::SetNextItemWidth(100);ImGui::InputInt(label("FITS every N frames (0 off)","FITS ogni N frame (0 spento)"),&o.checkpointFrames);o.checkpointFrames=std::clamp(o.checkpointFrames,0,1000);
        if(ImGui::TreeNode(label("Calibration / PC folders","Calibrazione / cartelle PC"))){
            ImGui::TextWrapped("%s",label("Folders of matching CR2s. Flats require bias or dark-flats. Dark exposure must match. Leave empty to disable.","Cartelle di CR2 compatibili. I flat richiedono bias o dark-flat. Esposizione dark identica. Vuoto per disabilitare."));
            for(auto [name,path]:std::initializer_list<std::pair<const char*,std::string*>>{{"Bias",&o.biasDirectory},{"Dark",&o.darkDirectory},{"Flat",&o.flatDirectory},{"Dark-flat",&o.darkFlatDirectory}}){ImGui::TextUnformatted(name);ImGui::SetNextItemWidth(-1);ImGui::PushID(name);ImGui::InputText("##folder",path);ImGui::PopID();}ImGui::TreePop();
        }
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
        ImGui::SetNextItemWidth(100);ImGui::InputInt("Memory (MiB)",&o.memoryLimitMb,128);o.memoryLimitMb=std::clamp(o.memoryLimitMb,512,16384);
        ImGui::TextWrapped("%s",label("Uses current Camera settings. RAW, M and fixed ISO required. Stop finishes the current exposure.","Usa i parametri attuali della fotocamera. Richiede RAW, M e ISO fisso. Stop completa lo scatto in corso."));
    }
    ImGui::EndDisabled();
    if(st.active&&st.exposureSeconds>.25){char text[80];std::snprintf(text,sizeof(text),"%.1f / %.1f s",st.exposureElapsed,st.exposureSeconds);ImGui::ProgressBar(st.phase=="exposing"?float(st.exposureElapsed/st.exposureSeconds):0,{-1,0},text);}
    ImGui::Text("Frame HFD %.2f / FWHM %.2f px",st.hfd,st.fwhm);ImGui::Text("Stack HFD %.2f / FWHM %.2f px",st.stackHfd,st.stackFwhm);ImGui::TextWrapped("%s",translatedMessage(st.detail).c_str());
    if(!st.astrometry.empty())ImGui::TextWrapped("%s",st.astrometry.c_str());
    if(!st.finalFile.empty())ImGui::TextWrapped("%s",st.finalFile.c_str());
}
void MainWindow::stackLevels(const Snapshot& s){
    const auto& st=s.stack;const bool english=config_.language=="en";
    auto label=[&](const char* en,const char* it){return english?en:it;};
    const float width=std::max(1.f,ImGui::GetContentRegionAvail().x);
    ImGui::BeginGroup();ImGui::TextUnformatted(label("Display histogram","Istogramma visualizzazione"));
    if(st.preview)ImGui::PlotHistogram("##stackhist",st.preview->histogram.data(),256,0,"Y / log count",0,FLT_MAX,{width,ImGui::GetFontSize()*5});
    else ImGui::Dummy({width,ImGui::GetFontSize()*5});
    bool changed=ImGui::Checkbox("Asinh stretch",&stackDisplay_.asinh);
    if(ImGui::BeginTable("##levels",2,ImGuiTableFlags_SizingStretchProp)){
        ImGui::TableSetupColumn("Level",ImGuiTableColumnFlags_WidthFixed,ImGui::GetFontSize()*3);ImGui::TableSetupColumn("Value");
        ImGui::TableNextRow();ImGui::TableNextColumn();ImGui::TextUnformatted("Low");ImGui::TableNextColumn();ImGui::SetNextItemWidth(-1);changed|=ImGui::SliderFloat("##low",&stackDisplay_.low,0,std::max(0.f,stackDisplay_.high-.0001f),"%.4f");
        ImGui::TableNextRow();ImGui::TableNextColumn();ImGui::TextUnformatted("Mid");ImGui::TableNextColumn();ImGui::SetNextItemWidth(-1);changed|=ImGui::SliderFloat("##mid",&stackDisplay_.mid,.002f,.998f,"%.3f",ImGuiSliderFlags_Logarithmic);
        ImGui::TableNextRow();ImGui::TableNextColumn();ImGui::TextUnformatted("High");ImGui::TableNextColumn();ImGui::SetNextItemWidth(-1);changed|=ImGui::SliderFloat("##high",&stackDisplay_.high,stackDisplay_.low+.0001f,1,"%.4f",ImGuiSliderFlags_Logarithmic);
        ImGui::EndTable();
    }
    if(ImGui::SmallButton(label("Reset display","Ripristina livelli"))){stackDisplay_={};changed=true;}
    if(changed){Command c;c.action=Action::StackStretch;c.pointU=stackDisplay_.low;c.pointV=stackDisplay_.mid;c.value=std::uint32_t(stackDisplay_.high*1000000);c.asinhStretch=stackDisplay_.asinh;mailbox_.send(c);}ImGui::EndGroup();
}
}
