#pragma once
#include <astro/Core.hpp>
#include <astro/Translation.hpp>
#include <imgui.h>
#include <imgui_stdlib.h>
#include <algorithm>
#include <cstdio>
namespace astro::ui::widgets {
inline float em(float n=1) {return ImGui::GetFontSize()*n;}
inline bool active(const Snapshot& s) {return s.autofocus.active()||s.progress.state==SequenceState::Running||s.progress.state==SequenceState::Paused||s.progress.state==SequenceState::Stopping;}
inline bool canDriveFocus(const Snapshot& s) {return s.connected&&s.liveView&&!active(s)&&s.lensFocus.state==LensFocusState::Ready;}
inline const char* focusDriveReason(const Snapshot& s) {
    if(!s.connected)return tr("Fotocamera non connessa");
    if(s.autofocus.active())return tr("Autofocus stellare in corso");
    if(!s.liveView)return tr("Attivare Live View");
    if(active(s))return tr("Scatto in corso");
    switch(s.lensFocus.state){case LensFocusState::NoLens:return tr("Nessun obiettivo elettronico rilevato");case LensFocusState::ManualFocus:return tr("Obiettivo in MF: impostare AF sulla lente");case LensFocusState::Ready:return tr("AF: movimento motore disponibile");case LensFocusState::Unsupported:return tr("Movimento focus non supportato");default:return tr("Stato AF/MF non disponibile");}
}
inline void muted(const char* text) {ImGui::PushStyleColor(ImGuiCol_Text,ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));ImGui::TextWrapped("%s",text);ImGui::PopStyleColor();}
inline void help(const char* text) {if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))ImGui::SetTooltip("%s",text);}
inline std::string label(const CameraProperty& p,std::uint32_t value) {
    for(const auto& choice:p.choices)if(choice.value==value)return tr(choice.label.c_str());
    if(p.value==value&&!p.currentLabel.empty())return tr(p.currentLabel.c_str());
    return tr("Non disponibile");
}
inline const char* propertyTitle(Property p) {
    switch(p) {
    case Property::Iso:return "ISO";case Property::Aperture:return tr("Apertura (Av)");case Property::Shutter:return tr("Tempo (Tv)");
    case Property::ExposureMode:return tr("Modalita AE");case Property::WhiteBalance:return tr("Bilanciamento");
    case Property::ColorTemperature:return tr("Temperatura K");case Property::PictureStyle:return tr("Stile immagine");case Property::ImageQuality:return tr("Formato / qualita");
    }return tr("Proprieta");
}
inline const CameraProperty* property(const Snapshot& s,Property id) {
    auto it=std::find_if(s.properties.begin(),s.properties.end(),[&](const auto& p){return p.id==id;});return it==s.properties.end()?nullptr:&*it;
}
inline bool form(const char* id,float labelWidth=9) {
    if(!ImGui::BeginTable(id,2,ImGuiTableFlags_SizingStretchProp))return false;
    ImGui::TableSetupColumn(tr("Campo"),ImGuiTableColumnFlags_WidthFixed,std::min(em(labelWidth),ImGui::GetContentRegionAvail().x*0.5f));ImGui::TableSetupColumn(tr("Valore"),ImGuiTableColumnFlags_WidthStretch);return true;
}
inline void field(const char* name) {ImGui::TableNextRow();ImGui::TableNextColumn();ImGui::AlignTextToFramePadding();ImGui::TextWrapped("%s",name);ImGui::TableNextColumn();ImGui::SetNextItemWidth(-1);}
inline void sameLineIfFits(const char* text) {
    const float right=ImGui::GetItemRectMax().x;
    const float limit=ImGui::GetWindowPos().x+ImGui::GetWindowContentRegionMax().x;
    if(right+ImGui::GetStyle().ItemSpacing.x+ImGui::CalcTextSize(text).x+2*ImGui::GetStyle().FramePadding.x<limit)ImGui::SameLine();
}
inline bool combo(const CameraProperty& p,std::uint32_t& value) {
    bool changed=false;ImGui::BeginDisabled(!p.writable);ImGui::SetNextItemWidth(-1);
    if(ImGui::BeginCombo("##property",label(p,value).c_str())) {
        for(const auto& choice:p.choices) {ImGui::PushID(static_cast<int>(choice.value));if(ImGui::Selectable(tr(choice.label.c_str()),choice.value==value)){value=choice.value;changed=true;}if(choice.value==value)ImGui::SetItemDefaultFocus();ImGui::PopID();}
        ImGui::EndCombo();
    }ImGui::EndDisabled();return changed;
}
inline void destinationChoice(Config& config,bool running) {
    ImGui::BeginDisabled(running);
    const char* labels[]{tr("Solo PC"),tr("PC + scheda")};int selected=config.saveDestination==SaveDestination::PcOnly?0:1;
    ImGui::SetNextItemWidth(-1);
    if(ImGui::Combo("##save-destination",&selected,labels,2))config.saveDestination=selected==0?SaveDestination::PcOnly:SaveDestination::PcAndCamera;
    help(tr("Si applica al prossimo scatto o sequenza. Solo PC evita la scrittura sulla scheda; non elimina foto esistenti."));
    ImGui::EndDisabled();
}
}
