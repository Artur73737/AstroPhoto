#include <astro/MainWindow.hpp>
#include "Widgets.hpp"
namespace astro::ui {
using namespace widgets;
void MainWindow::logs(const Snapshot& s) {
    ImGui::SetNextItemWidth(std::min(em(16),ImGui::GetContentRegionAvail().x));ImGui::InputTextWithHint("##filter",tr("Filtra messaggi..."),&filter_);
    sameLineIfFits(tr("Tutti i livelli      "));ImGui::SetNextItemWidth(em(10));const char* levelNames[]={tr("Tutti i livelli"),"Info","Warning","Error"};ImGui::Combo("##level",&logLevel_,levelNames,4);
    sameLineIfFits("Auto-scroll  ");ImGui::Checkbox("Auto-scroll",&autoScroll_);
    if(ImGui::BeginTable("log-entries",3,ImGuiTableFlags_RowBg|ImGuiTableFlags_Resizable|ImGuiTableFlags_ScrollY,{0,0})) {
        ImGui::TableSetupColumn(tr("Ora"),ImGuiTableColumnFlags_WidthFixed,em(5.2f));ImGui::TableSetupColumn(tr("Livello"),ImGuiTableColumnFlags_WidthFixed,em(4.8f));ImGui::TableSetupColumn(tr("Messaggio"),ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupScrollFreeze(0,1);ImGui::TableHeadersRow();
        const bool bottom=ImGui::GetScrollY()>=ImGui::GetScrollMaxY()-em(1);
        const char* levels[]={"","Info","Warning","Error"};
        for(const auto& entry:s.logs) {
            if(logLevel_&&entry.level!=levels[logLevel_])continue;if(!filter_.empty()&&entry.message.find(filter_)==std::string::npos)continue;
            const auto color=entry.level=="Error"?ImVec4{1,0.50f,0.40f,1}:entry.level=="Warning"?ImVec4{0.94f,0.74f,0.40f,1}:ImGui::GetStyleColorVec4(ImGuiCol_Text);
            ImGui::TableNextRow();ImGui::TableNextColumn();ImGui::TextUnformatted(entry.time.size()>11?entry.time.c_str()+11:entry.time.c_str());help(entry.time.c_str());
            ImGui::TableNextColumn();ImGui::TextColored(color,"%s",entry.level.c_str());ImGui::TableNextColumn();ImGui::PushStyleColor(ImGuiCol_Text,color);ImGui::TextWrapped("%s",entry.message.c_str());ImGui::PopStyleColor();
        }
        if(autoScroll_&&bottom)ImGui::SetScrollHereY(1);ImGui::EndTable();
    }
}
}
