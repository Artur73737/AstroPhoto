#pragma once
#include <astro/Core.hpp>
#include <astro/DesktopLayout.hpp>
#include <astro/PreviewAnalysis.hpp>
namespace astro::ui {
struct Preview {std::uint64_t texture{};int width{},height{};std::string status;const Histogram* histogram{};FocusReading focus;};
class MainWindow {
public:
    MainWindow(Mailbox&,Config,bool layoutLoaded=false);
    void render(const Snapshot&,Preview,float monitorScale,Preview captured={},DesktopArea desktop={});
    bool lightTheme() const { return config_.lightTheme; }
    bool focusPeaking() const { return config_.focusPeaking; }
    bool starPeaking() const { return config_.starPeaking; }
    float peakingThreshold() const { return config_.peakingThreshold; }
    int takeWindowAction() { const int action=windowAction_;windowAction_=0;return action; }
    float uiScale() const { return config_.uiScale; }
    bool exitRequested() const { return exitRequested_; }
    FocusTarget focusTarget() const {return focusTarget_;}
    void resetLayout() {resetLayout_=true;}
private:
    void send(Action,std::uint32_t value=0);
    void save();
    void start(const Snapshot&);
    void single(const Snapshot&);
    void connection(const Snapshot&);
    void settings(const Snapshot&);
    void sequence(const Snapshot&);
    void liveView(const Snapshot&,Preview,Preview);
    void focusControls(const Snapshot&);
    void logs(const Snapshot&);
    void output(const Snapshot&);
    Mailbox& mailbox_;Config config_;
    std::string output_,error_,filter_;
    int windowAction_{};
    bool viewLive_{};std::uint64_t seenCapture_{};
    int selectedRow_{-1},logLevel_{};
    bool autoScroll_{true};
    bool showConnection_{true},showSettings_{true},showLive_{true},showSequence_{true},showOutput_{true},showLog_{true};
    bool resetLayout_{true},exitRequested_{};
    bool showHistogram_{true},selectFocus_{};
    int focusStep_{1};
    FocusTarget focusTarget_;
    DesktopArea previousDesktop_;
    float previousScale_{};
};
void applyTheme(bool light=false);
}
