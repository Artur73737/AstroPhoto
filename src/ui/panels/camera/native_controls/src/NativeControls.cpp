#include <astro/ui/workspace/MainWindow.hpp>
#include <astro/ui/widgets/forms/Widgets.hpp>
#include <algorithm>
#include <cmath>
#include <climits>

namespace astro::ui {
using namespace widgets;
void MainWindow::nativeControls(const Snapshot& s) {
    if (!s.connected || nativeControlModel_ != s.model) {
        nativeControlEdits_.clear();
        nativeControlModel_ = s.model;
    }
    if (s.deviceControls.empty()) return;
    const bool english = config_.language == "en";
    const auto prefix = s.cameraModule.id + "/" + s.model + "/";
    auto preferences = [&](const DeviceControl& c) -> NativeControlUi& {
        auto [it, inserted] = config_.nativeControlUi.try_emplace(prefix + std::to_string(c.id));
        if (inserted) {
            it->second.logarithmic = c.logarithmic;
            it->second.order = c.id;
        }
        return it->second;
    };
    for (const auto& c : s.deviceControls) (void)preferences(c);
    ImGui::TextWrapped("%s", s.model.c_str());
    if (ImGui::Button(english ? "Customize controls" : "Personalizza controlli", {-1, 0}))
        ImGui::OpenPopup("native-customize");
    if (ImGui::BeginPopup("native-customize")) {
        ImGui::Checkbox(english ? "Apply on release / Enter" : "Applica al rilascio / Invio", &config_.nativeApplyOnRelease);
        ImGui::Separator();
        for (const auto& c : s.deviceControls) {
            ImGui::PushID(c.id);
            bool visible = !preferences(c).hidden;
            if (ImGui::Checkbox(c.name.c_str(), &visible)) preferences(c).hidden = !visible;
            ImGui::PopID();
        }
        ImGui::Separator();
        if (ImGui::Button(english ? "Reset layout" : "Ripristina disposizione")) {
            for (const auto& c : s.deviceControls) config_.nativeControlUi.erase(prefix + std::to_string(c.id));
        }
        if (ImGui::Button(english ? "Save preferences" : "Salva preferenze")) save();
        ImGui::EndPopup();
    }
    const char* categories[]{"Camera controls", "Image controls", "Auto exposure and miscellaneous", "Temperature and cooling"};
    const char* titlesIt[]{"Controlli camera", "Controlli immagine", "Esposizione Auto e altri controlli", "Temperatura e raffreddamento"};
    for (int category = 0; category < 5; ++category) {
        std::vector<const DeviceControl*> controls;
        for (const auto& c : s.deviceControls) {
            const auto found = std::find(std::begin(categories), std::end(categories), c.category);
            const int group = found == std::end(categories) ? 4 : int(found - std::begin(categories));
            if (group == category && !preferences(c).hidden) controls.push_back(&c);
        }
        if (controls.empty()) continue;
        std::stable_sort(controls.begin(), controls.end(), [&](auto a, auto b) {
            return preferences(*a).order < preferences(*b).order;
        });
        const auto sectionId = "native_group_" + std::to_string(category);
        const char* title = category == 4 ? (english ? "Additional controls" : "Controlli aggiuntivi")
                                          : (english ? categories[category] : titlesIt[category]);
        if (!section(sectionId.c_str(), title)) continue;
        for (const auto* ptr : controls) {
            const auto& c = *ptr;
            auto& pref = preferences(c);
            auto& edit = nativeControlEdits_[c.id];
            if (edit.pending) {
                if (auto result = mailbox_.result(edit.pending); result && result->finished) {
                    if (!result->error.empty()) error_ = result->error;
                    edit.pending = 0;
                    edit.dirty = false;
                }
            }
            if (!edit.initialized || (!edit.dirty && !edit.pending)) {
                edit.value = c.value;
                edit.automatic = c.automatic;
                edit.initialized = true;
            }
            ImGui::PushID(c.id);
            ImGui::TextWrapped("%s", c.name.c_str());
            help(c.description.c_str());
            bool commit = false, explicitCommit = false;
            // The context menu changes presentation only; camera capabilities
            // and range checks always come from the SDK snapshot.
            if (ImGui::BeginPopupContextItem("presentation")) {
                ImGui::TextWrapped("%s", c.description.c_str());
                ImGui::Text("SDK: %lld .. %lld", (long long)c.minimum, (long long)c.maximum);
                ImGui::Combo(english ? "Widget" : "Controllo", &pref.widget, "Automatic\0Slider\0Numeric\0");
                ImGui::Checkbox(english ? "Logarithmic slider" : "Slider logaritmico", &pref.logarithmic);
                ImGui::InputInt(english ? "Order" : "Ordine", &pref.order);
                pref.order = std::clamp(pref.order, -10000, 10000);
                ImGui::InputScalar(english ? "Step (SDK units)" : "Passo (unita SDK)", ImGuiDataType_S64, &pref.step);
                pref.step = std::clamp<std::int64_t>(pref.step, 1, INT_MAX);
                if (c.writable && ImGui::MenuItem(english ? "Camera default" : "Default camera", nullptr, false,
                                                   !active(s) && !edit.pending)) {
                    edit.value = c.defaultValue;
                    edit.automatic = false;
                    edit.dirty = commit = explicitCommit = true;
                }
                if (ImGui::MenuItem(english ? "Hide control" : "Nascondi controllo")) pref.hidden = true;
                if (ImGui::MenuItem(english ? "Save preferences" : "Salva preferenze")) save();
                ImGui::EndPopup();
            }
            ImGui::BeginDisabled(!s.connected || active(s) || !c.writable || edit.pending != 0);
            const auto low = c.minimum, high = c.maximum;
            if (!c.writable) {
                ImGui::Text("%.1f %s", c.value * c.displayScale, c.unit.c_str());
            } else if (!c.choices.empty() && pref.widget == 0) {
                std::string selected = std::to_string(edit.value);
                for (const auto& choice : c.choices) if (choice.value == edit.value) selected = choice.label;
                ImGui::SetNextItemWidth(-1);
                if (ImGui::BeginCombo("##choice", selected.c_str())) {
                    for (const auto& choice : c.choices)
                        if (choice.value >= low && choice.value <= high &&
                            ImGui::Selectable(choice.label.c_str(), choice.value == edit.value)) {
                            edit.value = choice.value;
                            edit.automatic = false;
                            edit.dirty = commit = true;
                        }
                    ImGui::EndCombo();
                }
            } else {
                const double factors[]{1, 1000, 1000000};
                const double factor = factors[pref.exposureUnit];
                if (c.unit == "us") {
                    auto display = edit.value / factor;
                    ImGui::SetNextItemWidth(std::max(em(4), ImGui::GetContentRegionAvail().x - em(5)));
                    const bool changed = ImGui::InputDouble("##duration", &display, 0, 0, "%.6f");
                    const bool released = ImGui::IsItemDeactivatedAfterEdit();
                    // Preserve a draft while typing; only release/Enter sends
                    // an exact integer SDK microsecond value, never ms.
                    if (changed && std::isfinite(display) && std::abs(display * factor) <= 9007199254740991.) {
                        edit.value = std::llround(display * factor);
                        edit.automatic = false;
                        edit.dirty = true;
                    }
                    commit |= released;
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(-1);
                    ImGui::Combo("##unit", &pref.exposureUnit, "us\0ms\0s\0");
                } else {
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::InputScalar("##number", ImGuiDataType_S64, &edit.value, &pref.step)) {
                        edit.dirty = true;
                        edit.automatic = false;
                    }
                    commit |= ImGui::IsItemDeactivatedAfterEdit();
                }
                if (pref.widget != 2) {
                    auto flags = ImGuiSliderFlags_AlwaysClamp | (pref.logarithmic && low >= 0 ? ImGuiSliderFlags_Logarithmic : 0);
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::SliderScalar("##slider", ImGuiDataType_S64, &edit.value, &low, &high, "%lld", flags)) {
                        if (pref.step > 1)
                            edit.value = std::clamp(low + ((edit.value - low + pref.step / 2) / pref.step) * pref.step, low, high);
                        edit.automatic = false;
                        edit.dirty = true;
                    }
                    commit |= ImGui::IsItemDeactivatedAfterEdit();
                }
                if (c.unit == "us") {
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::BeginCombo(english ? "##quick-picks" : "##scelte-rapide", english ? "Quick picks" : "Scelte rapide")) {
                        // Preferred 1/2/5 ticks are generated inside the actual
                        // camera's range, not a fabricated list of capabilities.
                        for (std::int64_t decade = 1; decade <= high; decade *= 10) {
                            for (int multiplier : {1, 2, 5}) {
                                const auto value = decade * multiplier;
                                if (value < low || value > high) continue;
                                char text[64];
                                std::snprintf(text, sizeof(text), "%.6g %s", value / factor, pref.exposureUnit == 0 ? "us" : pref.exposureUnit == 1 ? "ms" : "s");
                                if (ImGui::Selectable(text, edit.value == value)) {
                                    edit.value = value;
                                    edit.automatic = false;
                                    edit.dirty = commit = true;
                                }
                            }
                            if (decade > high / 10) break;
                        }
                        ImGui::EndCombo();
                    }
                }
            }
            if (c.autoSupported) {
                if (ImGui::Checkbox("Auto", &edit.automatic)) edit.dirty = commit = true;
            }
            const bool valid = edit.value >= low && edit.value <= high;
            if (edit.dirty && (!config_.nativeApplyOnRelease || !valid)) {
                ImGui::BeginDisabled(!valid);
                if (ImGui::Button(english ? "Apply" : "Applica", {-1, 0})) commit = explicitCommit = true;
                ImGui::EndDisabled();
            }
            ImGui::EndDisabled();
            if (commit && edit.dirty && valid && s.connected && !active(s) && c.writable && !edit.pending &&
                (config_.nativeApplyOnRelease || explicitCommit)) {
                // Manual Apply and automatic release both pass the same core
                // safety gate. No command is sent for every drag/mouse frame.
                Command command;
                command.action = Action::SetDeviceControl;
                command.controlId = c.id;
                command.controlValue = edit.value;
                command.controlAutomatic = edit.automatic;
                edit.pending = mailbox_.submit(std::move(command));
            }
            if (c.displayScale == 1)
                ImGui::TextWrapped("Camera: %lld%s %s", (long long)c.value, c.automatic ? " Auto" : "", c.unit.c_str());
            else
                ImGui::TextDisabled("SDK: %lld (x %.3g %s)", (long long)c.value, c.displayScale, c.unit.c_str());
            help((std::to_string(low) + " .. " + std::to_string(high) + " " + c.unit + " | " + c.description).c_str());
            if (!valid) muted(english ? "Value outside camera range" : "Valore fuori intervallo camera");
            ImGui::Spacing();
            ImGui::PopID();
        }
    }
}
}  // namespace astro::ui
