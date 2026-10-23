#pragma once
#include <astro/Core.hpp>
namespace astro::ui {
struct Preview {std::uint64_t texture{};int width{},height{};std::string status;};
struct DesktopArea {float x{},y{},width{},height{};};
class MainWindow {
public:
    MainWindow(Mailbox&,Config,bool layoutLoaded=false);
    void render(const Snapshot&,Preview,float monitorScale,Preview captured={},DesktopArea desktop={});
    bool lightTheme() const { return config_.lightTheme; }
    bool focusPeaking() const { return config_.focusPeaking; }
    float peakingThreshold() const { return config_.peakingThreshold; }
    int takeWindowAction() { const int action=windowAction_;windowAction_=0;return action; }
    float uiScale() const { return config_.uiScale; }
    bool exitRequested() const { return exitRequested_; }
private:
    void send(Action,std::uint32_t value=0);
    void save();
    void start(const Snapshot&);
    void single(const Snapshot&);
    void connection(const Snapshot&);
    void settings(const Snapshot&);
    void sequence(const Snapshot&);
    void liveView(const Snapshot&,Preview,Preview);
    void logs(const Snapshot&);
    void output();
    Mailbox& mailbox_;Config config_;
    std::string output_,error_,filter_;
    int windowAction_{};
    bool viewLive_{};std::uint64_t seenCapture_{};
    int selectedRow_{-1},logLevel_{};
    bool autoScroll_{true},crosshair_{true};
    bool showConnection_{true},showSettings_{true},showLive_{true},showSequence_{true},showOutput_{true},showLog_{true};
    bool resetLayout_{true},exitRequested_{};
};
void applyTheme(bool light=false);
}
