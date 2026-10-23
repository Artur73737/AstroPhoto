#include <astro/MainWindow.hpp>
#include "Widgets.hpp"
namespace astro::ui {
using namespace widgets;
void MainWindow::connection(const Snapshot& s) {
    ImGui::BeginDisabled(!s.backendAvailable||active(s));
    if(ImGui::Button(s.reconnecting?tr("Annulla riconnessione"):s.connected?tr("Disconnetti"):tr("Connetti"),{-1,0}))send(s.connected||s.reconnecting?Action::Disconnect:Action::Connect);
    ImGui::EndDisabled();
    if(form("connection",6)) {
        field(tr("Stato"));ImGui::TextUnformatted(s.reconnecting?tr("Riconnessione..."):s.connected?tr("Connessa"):tr("Disconnessa"));
        field(tr("Modello"));ImGui::TextWrapped("%s",s.model.empty()?"--":s.model.c_str());
        field("Driver");ImGui::TextWrapped("%s",s.backendAvailable?"Canon EDSDK":tr("Non disponibile"));ImGui::EndTable();
    }
    if(!s.backendAvailable){ImGui::Separator();muted(tr("Installare il pacchetto Canon EDSDK per abilitare la connessione USB."));}
    else if(!s.connected)muted(tr("Accendere la camera e chiudere EOS Utility."));
    if(ImGui::CollapsingHeader(tr("Dettagli")))ImGui::TextWrapped("%s",tr(s.backendStatus.c_str()));
}
void MainWindow::settings(const Snapshot& s) {
    if(ImGui::CollapsingHeader(tr("Scatto singolo"),ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::BeginDisabled(active(s));
        auto& item=config_.singleCapture;
        if(form("single",9)) {
            field(tr("Tipo frame"));int type=static_cast<int>(item.type);
            if(ImGui::Combo("##single-type",&type,"Light\0Dark\0Flat\0Bias\0"))item.type=static_cast<FrameType>(type);
            field("Bulb");ImGui::Checkbox("##single-bulb",&item.bulb);
            if(item.bulb){field(tr("Durata (s)"));ImGui::InputDouble("##single-duration",&item.exposureSeconds,0,0,"%.2f");}
            field(tr("Ritardo (s)"));ImGui::InputDouble("##single-delay",&item.delaySeconds,0,0,"%.2f");ImGui::EndTable();
        }
        if(item.type==FrameType::Dark)muted(tr("Dark: coprire l'obiettivo prima dello scatto."));
        ImGui::BeginDisabled(!s.connected);if(ImGui::Button(tr("Scatta"),{-1,0}))single(s);ImGui::EndDisabled();ImGui::EndDisabled();
        if(active(s)){if(ImGui::Button(tr("Stop scatto"),{-1,0}))send(Action::Stop);}
        if(!error_.empty())ImGui::TextWrapped("%s",error_.c_str());
    }
    ImGui::BeginDisabled(!s.connected||active(s));if(ImGui::Button(tr("Aggiorna"),{-1,0}))send(Action::Refresh);ImGui::EndDisabled();
    auto group=[&](const char* title,std::initializer_list<Property> props) {
        if(!ImGui::CollapsingHeader(title,ImGuiTreeNodeFlags_DefaultOpen))return;
        ImGui::PushID(title);if(form("properties",10)) {
            for(auto id:props) {
                ImGui::PushID(static_cast<int>(id));field(propertyTitle(id));const auto* p=property(s,id);
                if(p){auto value=p->value;ImGui::BeginDisabled(active(s));if(combo(*p,value)){Command c;c.action=Action::SetProperty;c.property=id;c.value=value;mailbox_.send(std::move(c));}ImGui::EndDisabled();}
                else {ImGui::BeginDisabled();if(ImGui::BeginCombo("##missing",s.connected?tr("Non supportata"):"--"))ImGui::EndCombo();ImGui::EndDisabled();}
                ImGui::PopID();
            }ImGui::EndTable();
        }ImGui::PopID();
    };
    group(tr("Esposizione"),{Property::ExposureMode,Property::Iso,Property::Aperture,Property::Shutter});
    group(tr("Colore"),{Property::WhiteBalance,Property::ColorTemperature,Property::PictureStyle});
    group(tr("Registrazione"),{Property::ImageQuality});
    if(ImGui::CollapsingHeader(tr("Salvataggio"),ImGuiTreeNodeFlags_DefaultOpen))destinationChoice(config_,active(s));
}
}
