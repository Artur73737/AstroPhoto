#include <astro/MainWindow.hpp>
#include "Widgets.hpp"
namespace astro::ui {
using namespace widgets;
void MainWindow::sequence(const Snapshot& s) {
    const bool running=active(s);
    ImGui::BeginDisabled(running||!s.connected||config_.sequence.empty());if(ImGui::Button("Avvia"))start(s);ImGui::EndDisabled();
    sameLineIfFits("Pausa");ImGui::BeginDisabled(!running);
    if(ImGui::Button(s.progress.state==SequenceState::Paused?"Riprendi":"Pausa"))send(s.progress.state==SequenceState::Paused?Action::Resume:Action::Pause);
    help("La pausa avviene prima dello scatto successivo.");sameLineIfFits("Stop");if(ImGui::Button("Stop"))send(Action::Stop);ImGui::EndDisabled();
    char progress[80];std::snprintf(progress,sizeof(progress),"%llu / %llu",static_cast<unsigned long long>(s.progress.completed),static_cast<unsigned long long>(s.progress.total));
    ImGui::ProgressBar(s.progress.total?static_cast<float>(s.progress.completed)/static_cast<float>(s.progress.total):0,{-1,0},progress);
    if(s.progress.pauseRequested)muted("Pausa richiesta...");
    if(!s.progress.detail.empty())ImGui::TextWrapped("%s",s.progress.detail.c_str());
    if(!error_.empty()){ImGui::PushStyleColor(ImGuiCol_Text,{1,0.48f,0.38f,1});ImGui::TextWrapped("%s",error_.c_str());ImGui::PopStyleColor();}
    ImGui::Separator();
    ImGui::BeginDisabled(running);
    if(ImGui::Button("+ Riga")){config_.sequence.push_back({});selectedRow_=static_cast<int>(config_.sequence.size()-1);}
    sameLineIfFits("Duplica");const bool selected=selectedRow_>=0&&selectedRow_<static_cast<int>(config_.sequence.size());
    ImGui::BeginDisabled(!selected);
    if(ImGui::Button("Duplica")){auto copy=config_.sequence.at(selectedRow_);config_.sequence.insert(config_.sequence.begin()+selectedRow_+1,copy);++selectedRow_;}
    sameLineIfFits("Elimina");if(ImGui::Button("Elimina")){config_.sequence.erase(config_.sequence.begin()+selectedRow_);selectedRow_=std::min(selectedRow_,static_cast<int>(config_.sequence.size())-1);}
    ImGui::EndDisabled();
    if(ImGui::BeginTable("plan",4,ImGuiTableFlags_BordersInnerV|ImGuiTableFlags_RowBg|ImGuiTableFlags_Resizable|ImGuiTableFlags_ScrollY,{0,em(8)})) {
        ImGui::TableSetupColumn("Tipo",ImGuiTableColumnFlags_WidthStretch,1.3f);
        ImGui::TableSetupColumn("N.",ImGuiTableColumnFlags_WidthStretch,0.5f);
        ImGui::TableSetupColumn("Modo",ImGuiTableColumnFlags_WidthStretch,1);
        ImGui::TableSetupColumn("Secondi",ImGuiTableColumnFlags_WidthStretch,1);
        ImGui::TableSetupScrollFreeze(0,1);ImGui::TableHeadersRow();
        for(std::size_t index=0;index<config_.sequence.size();++index) {
            const auto& item=config_.sequence[index];ImGui::PushID(static_cast<int>(index));ImGui::TableNextRow();ImGui::TableNextColumn();
            if(ImGui::Selectable(frameName(item.type).c_str(),selectedRow_==static_cast<int>(index),ImGuiSelectableFlags_SpanAllColumns))selectedRow_=static_cast<int>(index);
            ImGui::TableNextColumn();ImGui::Text("%d",item.count);ImGui::TableNextColumn();ImGui::TextUnformatted(item.bulb?"Bulb":"Tv");
            ImGui::TableNextColumn();if(item.bulb)ImGui::Text("%.2f",item.exposureSeconds);else ImGui::TextDisabled("--");ImGui::PopID();
        }ImGui::EndTable();
    }
    if(config_.sequence.empty())muted("Aggiungi una riga al piano.");
    if(selectedRow_>=0&&selectedRow_<static_cast<int>(config_.sequence.size())) {
        auto& item=config_.sequence.at(selectedRow_);
        if(ImGui::CollapsingHeader("Dettagli scatto",ImGuiTreeNodeFlags_DefaultOpen)) {
            if(form("shot",9)) {
                field("Tipo");int type=static_cast<int>(item.type);if(ImGui::Combo("##type",&type,"Light\0Dark\0Flat\0Bias\0"))item.type=static_cast<FrameType>(type);
                field("Quantita");ImGui::InputInt("##count",&item.count,0);
                field("Bulb");ImGui::Checkbox("##bulb",&item.bulb);
                field(item.bulb?"Durata (s)":"Attesa (s)");ImGui::InputDouble("##duration",&item.exposureSeconds,0,0,"%.3f");
                help(item.bulb?"Durata richiesta dell'esposizione Bulb.":"Budget di esposizione prima del timeout download. Il tempo reale e' impostato da Tv.");
                field("Attesa iniziale");ImGui::InputDouble("##delay",&item.delaySeconds,0,0,"%.2f");help("Attesa prima del primo scatto della riga, in secondi.");
                field("Intervallo (s)");ImGui::InputDouble("##interval",&item.intervalSeconds,0,0,"%.2f");help("Delay dal termine del download allo scatto successivo, anche tra righe. Zero = nessuna attesa aggiuntiva.");ImGui::EndTable();
            }
        }
        if(ImGui::CollapsingHeader("Impostazioni camera",ImGuiTreeNodeFlags_DefaultOpen)) {
            if(form("shot-camera",9)) {
                for(auto id:{Property::Iso,Property::Aperture,Property::Shutter}) {
                    field(propertyTitle(id));ImGui::PushID(static_cast<int>(id));const auto* p=property(s,id);
                    auto it=item.settings.find(id);const auto title=it==item.settings.end()?"Corrente":p?label(*p,it->second):"Collegare la camera";
                    if(ImGui::BeginCombo("##setting",title.c_str())) {
                        if(ImGui::Selectable("Corrente",it==item.settings.end()))item.settings.erase(id);
                        if(p)for(const auto& choice:p->choices) {ImGui::PushID(static_cast<int>(choice.value));if(ImGui::Selectable(choice.label.c_str(),item.settings.contains(id)&&item.settings.at(id)==choice.value))item.settings[id]=choice.value;ImGui::PopID();}
                        ImGui::EndCombo();
                    }ImGui::PopID();
                }ImGui::EndTable();
            }
        }
    }
    ImGui::EndDisabled();
}
void MainWindow::output() {
    ImGui::TextUnformatted("Cartella di destinazione");ImGui::SetNextItemWidth(-1);ImGui::InputText("##output",&output_);
    if(form("output-session",7)) {field("Sessione");ImGui::InputText("##session",&config_.sessionName);ImGui::EndTable();}
    if(ImGui::Button("Salva piano"))save();sameLineIfFits("CR2 / JPEG nativi");ImGui::TextDisabled("CR2 / JPEG nativi");
    if(ImGui::CollapsingHeader("Organizzazione file"))muted("Sessione / Light, Dark, Flat, Bias. Il formato dipende da ImageQuality sulla fotocamera. Conversione RAW non disponibile.");
    if(!error_.empty())ImGui::TextWrapped("%s",error_.c_str());
}
}
