#include <astro/MainWindow.hpp>
#include "Widgets.hpp"
namespace astro::ui {
using namespace widgets;
void MainWindow::connection(const Snapshot& s) {
    ImGui::BeginDisabled(!s.backendAvailable||active(s));
    if(ImGui::Button(s.connected?"Disconnetti":"Connetti",{-1,0}))send(s.connected?Action::Disconnect:Action::Connect);
    ImGui::EndDisabled();
    if(form("connection",6)) {
        field("Stato");ImGui::TextUnformatted(s.connected?"Connessa":"Disconnessa");
        field("Modello");ImGui::TextWrapped("%s",s.model.empty()?"--":s.model.c_str());
        field("Driver");ImGui::TextWrapped("%s",s.backendAvailable?"Canon EDSDK":"Non disponibile");ImGui::EndTable();
    }
    if(!s.backendAvailable){ImGui::Separator();muted("Installare il pacchetto Canon EDSDK per abilitare la connessione USB.");}
    else if(!s.connected)muted("Accendere la camera e chiudere EOS Utility.");
    if(ImGui::CollapsingHeader("Dettagli"))ImGui::TextWrapped("%s",s.backendStatus.c_str());
}
void MainWindow::settings(const Snapshot& s) {
    if(ImGui::CollapsingHeader("Scatto singolo",ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::BeginDisabled(active(s));
        auto& item=config_.singleCapture;
        if(form("single",9)) {
            field("Tipo frame");int type=static_cast<int>(item.type);
            if(ImGui::Combo("##single-type",&type,"Light\0Dark\0Flat\0Bias\0"))item.type=static_cast<FrameType>(type);
            field("Bulb");ImGui::Checkbox("##single-bulb",&item.bulb);
            if(item.bulb){field("Durata (s)");ImGui::InputDouble("##single-duration",&item.exposureSeconds,0,0,"%.2f");}
            field("Ritardo (s)");ImGui::InputDouble("##single-delay",&item.delaySeconds,0,0,"%.2f");ImGui::EndTable();
        }
        if(item.type==FrameType::Dark)muted("Dark: coprire l'obiettivo prima dello scatto.");
        ImGui::BeginDisabled(!s.connected);if(ImGui::Button("Scatta",{-1,0}))single(s);ImGui::EndDisabled();ImGui::EndDisabled();
        if(active(s)){if(ImGui::Button("Stop scatto",{-1,0}))send(Action::Stop);}
        if(!error_.empty())ImGui::TextWrapped("%s",error_.c_str());
    }
    ImGui::BeginDisabled(!s.connected||active(s));if(ImGui::Button("Aggiorna",{-1,0}))send(Action::Refresh);ImGui::EndDisabled();
    auto group=[&](const char* title,std::initializer_list<Property> props) {
        if(!ImGui::CollapsingHeader(title,ImGuiTreeNodeFlags_DefaultOpen))return;
        ImGui::PushID(title);if(form("properties",10)) {
            for(auto id:props) {
                ImGui::PushID(static_cast<int>(id));field(propertyTitle(id));const auto* p=property(s,id);
                if(p){auto value=p->value;ImGui::BeginDisabled(active(s));if(combo(*p,value)){Command c;c.action=Action::SetProperty;c.property=id;c.value=value;mailbox_.send(std::move(c));}ImGui::EndDisabled();}
                else {ImGui::BeginDisabled();if(ImGui::BeginCombo("##missing",s.connected?"Non supportata":"--"))ImGui::EndCombo();ImGui::EndDisabled();}
                ImGui::PopID();
            }ImGui::EndTable();
        }ImGui::PopID();
    };
    group("Esposizione",{Property::ExposureMode,Property::Iso,Property::Aperture,Property::Shutter});
    group("Colore",{Property::WhiteBalance,Property::ColorTemperature,Property::PictureStyle});
    group("Registrazione",{Property::ImageQuality});
}
}
