#pragma once
#include <astro/MainWindow.hpp>
#include <nlohmann/json.hpp>
namespace astro {
using ControlJson=nlohmann::json;
// Called only after rendering, on the thread owning the ImGui/GL contexts.
class ControlApi {
public:
    ControlApi(Mailbox& mailbox,ui::MainWindow& ui):mailbox_(mailbox),ui_(ui){}
    ControlJson call(const ControlJson&,const std::function<ControlJson(const ControlJson&)>& image);
private:
    Mailbox& mailbox_;ui::MainWindow& ui_;
};
ControlJson previewTelemetry(const Histogram&,const FocusReading&);
}
