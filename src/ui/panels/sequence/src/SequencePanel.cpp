#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include <astro/ui/workspace/MainWindow.hpp>
#include <cstdint>
#include <astro/ui/widgets/forms/Widgets.hpp>
namespace astro::ui {
using namespace widgets;
void MainWindow::sequence(const Snapshot& s) {
    const bool running = active(s);
    ImGui::BeginDisabled(running || !s.connected || config_.sequence.empty());
    if (ImGui::Button(tr("Avvia"))) start(s);
    ImGui::EndDisabled();
    sameLineIfFits(tr("Pausa"));
    ImGui::BeginDisabled(!running);
    if (ImGui::Button(s.progress.state == SequenceState::Paused ? tr("Riprendi") : tr("Pausa")))
        send(s.progress.state == SequenceState::Paused ? Action::Resume : Action::Pause);
    help(tr("La pausa avviene prima dello scatto successivo."));
    sameLineIfFits("Stop");
    if (ImGui::Button("Stop")) send(Action::Stop);
    ImGui::EndDisabled();
    char progress[80];
    std::snprintf(progress, sizeof(progress), "%llu / %llu", static_cast<unsigned long long>(s.progress.completed),
                  static_cast<unsigned long long>(s.progress.total));
    ImGui::ProgressBar(
        s.progress.total ? static_cast<float>(s.progress.completed) / static_cast<float>(s.progress.total) : 0, {-1, 0},
        progress);
    if (s.progress.pauseRequested) muted(tr("Pausa richiesta..."));
    if (!s.progress.detail.empty()) ImGui::TextWrapped("%s", translatedMessage(s.progress.detail).c_str());
    if (!error_.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, {1, 0.48f, 0.38f, 1});
        ImGui::TextWrapped("%s", error_.c_str());
        ImGui::PopStyleColor();
    }
    ImGui::Separator();
    ImGui::BeginDisabled(running);
    if (ImGui::Button(tr("+ Riga"))) {
        config_.sequence.push_back({});
        selectedRow_ = static_cast<int>(config_.sequence.size() - 1);
    }
    sameLineIfFits(tr("Duplica"));
    const bool selected = selectedRow_ >= 0 && selectedRow_ < static_cast<int>(config_.sequence.size());
    ImGui::BeginDisabled(!selected);
    if (ImGui::Button(tr("Duplica"))) {
        auto copy = config_.sequence.at(selectedRow_);
        config_.sequence.insert(config_.sequence.begin() + selectedRow_ + 1, copy);
        ++selectedRow_;
    }
    sameLineIfFits(tr("Elimina"));
    if (ImGui::Button(tr("Elimina"))) {
        config_.sequence.erase(config_.sequence.begin() + selectedRow_);
        selectedRow_ = std::min(selectedRow_, static_cast<int>(config_.sequence.size()) - 1);
    }
    ImGui::EndDisabled();
    if (ImGui::BeginTable(
            "plan", 4,
            ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY,
            {0, em(8)})) {
        ImGui::TableSetupColumn(tr("Tipo"), ImGuiTableColumnFlags_WidthStretch, 1.3f);
        ImGui::TableSetupColumn("N.", ImGuiTableColumnFlags_WidthStretch, 0.5f);
        ImGui::TableSetupColumn(tr("Modo"), ImGuiTableColumnFlags_WidthStretch, 1);
        ImGui::TableSetupColumn(tr("Secondi"), ImGuiTableColumnFlags_WidthStretch, 1);
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();
        for (std::size_t index = 0; index < config_.sequence.size(); ++index) {
            const auto& item = config_.sequence[index];
            ImGui::PushID(static_cast<int>(index));
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            if (ImGui::Selectable(frameName(item.type).c_str(), selectedRow_ == static_cast<int>(index),
                                  ImGuiSelectableFlags_SpanAllColumns))
                selectedRow_ = static_cast<int>(index);
            ImGui::TableNextColumn();
            ImGui::Text("%d", item.count);
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(item.bulb ? "Bulb" : "Tv");
            ImGui::TableNextColumn();
            if (item.bulb)
                ImGui::Text("%.2f", item.exposureSeconds);
            else
                ImGui::TextDisabled("--");
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    if (config_.sequence.empty()) muted(tr("Aggiungi una riga al piano."));
    if (selectedRow_ >= 0 && selectedRow_ < static_cast<int>(config_.sequence.size())) {
        auto& item = config_.sequence.at(selectedRow_);
        if (section("sequence_details", tr("Dettagli scatto"))) {
            if (form("shot", 9)) {
                field(tr("Tipo"));
                int type = static_cast<int>(item.type);
                if (ImGui::Combo("##type", &type, "Light\0Dark\0Flat\0Bias\0"))
                    item.type = static_cast<FrameType>(type);
                if (s.capture) {
                    field("Output file");
                    if (ImGui::BeginCombo("##output", item.outputFormat.c_str())) {
                        for (const auto& format : s.capture->outputFormats)
                            if (ImGui::Selectable(format.c_str(), format == item.outputFormat))
                                item.outputFormat = format;
                        ImGui::EndCombo();
                    }
                }
                field(tr("Quantita"));
                ImGui::InputInt("##count", &item.count, 0);
                if (!s.capture) {
                    field("Bulb");
                    ImGui::Checkbox("##bulb", &item.bulb);
                }
                if (!s.capture) {
                    field(item.bulb ? tr("Durata (s)") : tr("Attesa (s)"));
                    ImGui::InputDouble("##duration", &item.exposureSeconds, 0, 0, "%.3f");
                    help(item.bulb ? tr("Durata richiesta dell'esposizione Bulb.")
                                   : tr("Budget di esposizione prima del timeout download. Il tempo reale e' impostato "
                                        "da Tv."));
                } else {
                    field(config_.language == "en" ? "Exposure" : "Esposizione");
                    const auto* exposure = property(s, Property::ExposureMicroseconds);
                    ImGui::Text("%.6g s", exposure ? exposure->value / 1000000. : 0.);
                    help(config_.language == "en" ? "Uses the current native exposure in Camera controls."
                                                  : "Usa l'esposizione corrente dei controlli camera.");
                }
                field(tr("Attesa iniziale"));
                ImGui::InputDouble("##delay", &item.delaySeconds, 0, 0, "%.2f");
                help(tr("Attesa prima del primo scatto della riga, in secondi."));
                field(tr("Intervallo (s)"));
                ImGui::InputDouble("##interval", &item.intervalSeconds, 0, 0, "%.2f");
                help(
                    tr("Delay dal termine del download allo scatto successivo, anche tra righe. Zero = nessuna attesa "
                       "aggiuntiva."));
                ImGui::EndTable();
            }
        }
        if (section("sequence_camera", tr("Impostazioni camera")) && !s.capture) {
            if (form("shot-camera", 9)) {
                for (auto id : {Property::Iso, Property::Aperture, Property::Shutter}) {
                    field(propertyTitle(id));
                    ImGui::PushID(static_cast<int>(id));
                    const auto* p = property(s, id);
                    auto it = item.settings.find(id);
                    const auto title = it == item.settings.end() ? tr("Corrente")
                                       : p                       ? label(*p, it->second)
                                                                 : tr("Collegare la camera");
                    if (ImGui::BeginCombo("##setting", title.c_str())) {
                        if (ImGui::Selectable(tr("Corrente"), it == item.settings.end())) item.settings.erase(id);
                        if (p)
                            for (const auto& choice : p->choices) {
                                ImGui::PushID(static_cast<int>(choice.value));
                                if (ImGui::Selectable(
                                        tr(choice.label.c_str()),
                                        item.settings.contains(id) && item.settings.at(id) == choice.value))
                                    item.settings[id] = choice.value;
                                ImGui::PopID();
                            }
                        ImGui::EndCombo();
                    }
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
        }
    }
    ImGui::EndDisabled();
}
void MainWindow::openSessionFolder() {
    auto config = config_;
    config.outputDirectory = pathFromUtf8(output_);
    // Before the first capture of the day the session folder does not exist yet:
    // open the closest existing parent instead of failing.
    const auto folder = nearestExistingDirectory(sessionDirectory(config, std::chrono::system_clock::now()));
    if (folder.empty()) {
        error_ = tr("La cartella di destinazione non esiste");
        return;
    }
    const auto result = reinterpret_cast<std::intptr_t>(
        ShellExecuteW(nullptr, L"open", folder.wstring().c_str(), nullptr, nullptr, SW_SHOWNORMAL));
    if (result <= 32) error_ = tr("Impossibile aprire Esplora file");
}
void MainWindow::output(const Snapshot& s) {
    ImGui::TextUnformatted(tr("Salvataggio"));
    if (s.capture) {
        config_.saveDestination = SaveDestination::PcOnly;
        ImGui::TextUnformatted(tr("Solo PC"));
    } else
        destinationChoice(config_, active(s));
    ImGui::TextUnformatted(tr("Cartella di destinazione"));
    ImGui::SetNextItemWidth(-1);
    ImGui::InputText("##output", &output_);
    if (form("output-session", 7)) {
        field(tr("Sessione"));
        ImGui::InputText("##session", &config_.sessionName);
        ImGui::EndTable();
    }
    if (ImGui::Button(tr("Salva piano"))) save();
    ImGui::SameLine();
    if (ImGui::Button(tr("Apri cartella sessione"))) openSessionFolder();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tr("Apre in Esplora file la cartella della sessione di oggi"));
    const auto* formats = s.capture ? "SER / FITS / PNG / TIFF" : tr("CR2 / JPEG nativi");
    sameLineIfFits(formats);
    ImGui::TextDisabled("%s", formats);
    if (section("file_organization", tr("Organizzazione file"), false)) {
        if (s.capture)
            muted(
                config_.language == "en"
                    ? "Local date / Session / captures / frame type + capture ID. Native frames, capture.json and "
                      "frames.csv."
                    : "Data locale / Sessione / captures / tipo frame + ID. Frame nativi, capture.json e frames.csv.");
        else
            muted(
                tr("Data locale / Sessione / Light, Dark, Flat, Bias. Live stack: originali RAW, FITS lineare e "
                   "stack.json."));
    }
    if (!error_.empty()) ImGui::TextWrapped("%s", error_.c_str());
}
}  // namespace astro::ui
