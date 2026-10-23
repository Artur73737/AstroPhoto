#include <astro/MainWindow.hpp>
#include "Widgets.hpp"
#include <cmath>
namespace astro::ui {
using namespace widgets;
void MainWindow::focusControls(const Snapshot& s){
    if(ImGui::CollapsingHeader(tr("Motore e punto focus"),ImGuiTreeNodeFlags_DefaultOpen)){
        ImGui::BeginDisabled(!canDriveFocus(s));
        if(ImGui::Button("-##focus-near",{em(3),0}))send(Action::Focus,static_cast<unsigned>(3-focusStep_));
        help(tr("Verso vicino: un comando al motore dell'obiettivo"));ImGui::SameLine();
        if(ImGui::Button("+##focus-far",{em(3),0}))send(Action::Focus,static_cast<unsigned>(3+focusStep_));
        help(tr("Verso infinito: un comando al motore dell'obiettivo"));ImGui::EndDisabled();
        ImGui::SameLine();ImGui::SetNextItemWidth(-1);const char* steps[]{"1 - Minimo","2 - Medio","3 - Ampio"};
        if(ImGui::BeginCombo("##focus-step",tr(steps[focusStep_-1]))){for(int i=0;i<3;++i)if(ImGui::Selectable(tr(steps[i]),focusStep_==i+1))focusStep_=i+1;ImGui::EndCombo();}
        help(tr("Tre ampiezze Canon, non gradi ne multipli esatti. Ogni clic invia un solo movimento."));
        muted(focusDriveReason(s));
        ImGui::BeginDisabled(!viewLive_||s.autofocus.active());
        if(ImGui::Checkbox(tr("Punto focus"),&selectFocus_)){focusTarget_.enabled=false;++focusTarget_.revision;}
        sameLineIfFits(tr("Azzera focus"));if(ImGui::Button(tr("Azzera focus")))++focusTarget_.revision;
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!s.liveView||active(s));for(int ratio:{1,5,10}){if(ratio!=1)ImGui::SameLine();if(ImGui::RadioButton((std::to_string(ratio)+"x").c_str(),s.frame.zoom==ratio))send(Action::Zoom,ratio);}ImGui::EndDisabled();
    }
    if(ImGui::CollapsingHeader(tr("Peaking e overlay"),ImGuiTreeNodeFlags_DefaultOpen)){
        ImGui::Checkbox("Focus peaking",&config_.focusPeaking);sameLineIfFits(tr("Stelle"));ImGui::Checkbox(tr("Stelle"),&config_.starPeaking);
        if(config_.focusPeaking){ImGui::SetNextItemWidth(-1);ImGui::SliderFloat("##peaking-threshold",&config_.peakingThreshold,10,100,"%.0f");help(tr("Soglia bassa: piu bordi. Soglia alta: solo dettagli con contrasto forte."));}
        ImGui::Checkbox(tr("Istogrammi"),&showHistogram_);sameLineIfFits(tr("Reticolo"));ImGui::Checkbox(tr("Reticolo"),&config_.crosshairEnabled);
        const std::pair<const char*,const char*> shapes[]{{"cross","Croce"},{"gap_cross","Croce aperta"},{"circle","Cerchio"},{"target","Bersaglio"},{"thirds","Griglia terzi"}};
        const char* selected="Croce";for(auto [id,title]:shapes)if(config_.crosshairShape==id)selected=title;
        ImGui::SetNextItemWidth(-1);if(ImGui::BeginCombo("##reticle-shape",tr(selected))){for(auto [id,title]:shapes)if(ImGui::Selectable(tr(title),config_.crosshairShape==id))config_.crosshairShape=id;ImGui::EndCombo();}
    }
    if(ImGui::CollapsingHeader(tr("Autofocus stellare"),ImGuiTreeNodeFlags_DefaultOpen)){
        auto& o=config_.starFocus;const auto& f=s.autofocus;
        ImGui::Text(tr("Stelle valide: %d | sature: %d"),static_cast<int>(s.stars.stars.size()),s.stars.saturated);
        if(s.stars.acquisitionCandidates>static_cast<int>(s.stars.stars.size()))ImGui::Text(tr("Sorgenti sfocate: %d"),s.stars.acquisitionCandidates);
        if(!s.stars.stars.empty()){
            std::vector<double> sizes;for(auto star:s.stars.stars)sizes.push_back(star.hfd);std::sort(sizes.begin(),sizes.end());
            ImGui::Text(tr("HFD campo: %.2f px"),sizes[sizes.size()/2]);
        }
        ImGui::BeginDisabled(!canStartStarFocus(s));
        if(ImGui::Button(tr("Avvia AF stelle"))){Command c;c.action=Action::StartStarFocus;c.config=config_;mailbox_.send(std::move(c));viewLive_=true;}
        ImGui::EndDisabled();sameLineIfFits("Stop AF");ImGui::BeginDisabled(!f.active());if(ImGui::Button("Stop AF"))send(Action::StopStarFocus);ImGui::EndDisabled();
        if(!f.detail.empty()){
            const auto color=f.phase==StarFocusPhase::Complete?ImVec4(.3f,.9f,.5f,1):f.phase==StarFocusPhase::Failed?ImVec4(1,.45f,.3f,1):ImGui::GetStyleColorVec4(ImGuiCol_Text);
            ImGui::PushStyleColor(ImGuiCol_Text,color);ImGui::TextWrapped("%s",translatedMessage(f.detail).c_str());ImGui::PopStyleColor();
        }
        ImGui::Text("HFD %.2f px | min %.2f",f.current,f.best);
        if(f.verified>0)ImGui::Text(tr("Verifica: %.2f px"),f.verified);
        if(f.active())ImGui::Text(tr("Punti %d/%d | frame %d/%d"),static_cast<int>(f.curve.size()),o.positions,f.frames,o.framesPerPoint*(f.phase==StarFocusPhase::Verifying?2:1));
        if(f.active()||f.moves>0)ImGui::TextWrapped(tr("Passata %d/%d | passo %d | distanza %d | ricerca %d | mosse %d/%d"),f.pass,o.maxPasses,f.motorStep,f.stride,f.searchMoves,f.moves,o.maxMoves);
        if(!f.curve.empty()){
            std::vector<float> values;for(auto p:f.curve)values.push_back(static_cast<float>(p.hfd));
            ImGui::PlotLines("##star-curve",values.data(),static_cast<int>(values.size()),0,"HFD (px)",FLT_MAX,FLT_MAX,{ImGui::GetContentRegionAvail().x,em(5)});
            ImGui::TextDisabled("R2 %.2f | %s %d",f.curveR2,tr("movimenti"),f.moves);
        }
        if(ImGui::TreeNode(tr("Parametri AF"))){
            ImGui::BeginDisabled(f.active());
            ImGui::Checkbox(tr("Ricerca stelle automatica"),&o.searchEnabled);
            if(form("star-options",10)){
                field(tr("Passo ricerca"));ImGui::SliderInt("##search-step",&o.searchStep,1,3);
                field(tr("Raggio ricerca"));ImGui::SliderInt("##search-span",&o.searchSpan,2,40);
                field(tr("Passate massime"));ImGui::SliderInt("##max-passes",&o.maxPasses,1,8);
                field(tr("Distanza massima"));ImGui::SliderInt("##max-stride",&o.maxStride,1,16);
                field(tr("Passo Canon"));ImGui::SliderInt("##af-step",&o.step,1,3);
                field(tr("Posizioni"));if(ImGui::SliderInt("##af-positions",&o.positions,7,21))o.positions|=1;
                field(tr("Frame/punto"));ImGui::SliderInt("##af-frames",&o.framesPerPoint,3,15);
                field(tr("Stelle minime"));ImGui::SliderInt("##af-stars",&o.minStars,3,20);
                field(tr("Assestamento ms"));ImGui::SliderInt("##af-settle",&o.settleMs,200,5000);
                field(tr("Backlash passi"));ImGui::SliderInt("##af-backlash",&o.backlash,1,10);
                field(tr("SNR minimo"));ImGui::InputDouble("##af-snr",&o.detectionSnr,0,0,"%.1f");o.detectionSnr=std::clamp(o.detectionSnr,4.,20.);
                field(tr("Limite movimenti"));ImGui::SliderInt("##af-moves",&o.maxMoves,20,200);
                field(tr("Timeout (s)"));ImGui::SliderInt("##af-timeout",&o.timeoutSeconds,30,600);
                field(tr("Abbinamento px"));ImGui::InputDouble("##af-match",&o.matchRadius,0,0,"%.1f");o.matchRadius=std::clamp(o.matchRadius,2.,30.);
                field(tr("Dispersione max"));ImGui::InputDouble("##af-scatter",&o.maxScatter,0,0,"%.2f");o.maxScatter=std::clamp(o.maxScatter,.02,.25);
                field(tr("Tolleranza verifica"));ImGui::InputDouble("##af-verify",&o.verificationTolerance,0,0,"%.2f");o.verificationTolerance=std::clamp(o.verificationTolerance,.02,.20);
                ImGui::EndTable();
            }
            muted(tr("Ritorno sempre verso infinito. I passi non sono posizioni assolute. Stop lascia la lente nella posizione corrente."));
            if(ImGui::Button(tr("Salva impostazioni")))save();
            ImGui::EndDisabled();ImGui::TreePop();
        }
    }
    if(ImGui::CollapsingHeader(tr("Consigli esposizione"),ImGuiTreeNodeFlags_DefaultOpen)){
        if(!s.liveView)muted(tr("Avvia Live View per analizzare le stelle."));
        else if(!s.manualFocusExposure)muted(tr("Usa M, ISO fisso e Tv non Bulb. L'esposizione deve restare costante durante AF."));
        else if(s.stars.saturated>0||s.stars.clippedFraction>.01)muted(tr("Nuclei saturi: abbassa ISO o accorcia Tv; se serve chiudi il diaframma (numero f maggiore)."));
        else if(s.stars.background>100)muted(tr("Fondo troppo luminoso: abbassa ISO o accorcia Tv. Sul monitor riduci anche la luminosita."));
        else if(s.stars.stars.size()<static_cast<std::size_t>(config_.starFocus.minStars))muted(tr("Puoi avviare AF anche senza stelle: ricerca motorizzata nelle due direzioni. Se il campo resta buio, apri il diaframma, allunga Tv o aumenta ISO evitando saturazione."));
        else muted(tr("Stelle misurabili. Mantieni ISO, Tv, apertura, zoom e inquadratura costanti."));
        muted(tr("Consigli dal JPEG Live View: non misurano l'esposizione RAW. Il monitor non equivale al cielo reale."));
    }
}
}
