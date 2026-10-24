#pragma once
#include <astro/Core.hpp>
#include <astro/DesktopLayout.hpp>
#include <astro/PreviewAnalysis.hpp>
namespace astro::ui {
struct Preview {std::uint64_t texture{};int width{},height{};std::string status;const Histogram* histogram{};FocusReading focus;};
struct ControlState {
    bool viewLive{},histogram{true},selectFocus{},autoScroll{true};
    int focusStep{1},selectedRow{-1},logLevel{};std::string logFilter,error;
    std::array<bool,6> panels{};FocusTarget target;
    std::map<std::string,bool> sections;float focusScroll{};
};
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
    Config controlConfig() const;
    void controlConfig(const Config&);
    ControlState controlState() const;
    void controlState(const ControlState&);
    void controlWindow(int action) {windowAction_=action;}
    void controlExit() {exitRequested_=true;}
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
    bool section(const char* id,const char* title,bool defaultOpen=true,bool tree=false);
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
    std::map<std::string,bool> sections_{{"connection_details",false},{"single_capture",true},{"exposure",true},{"color",true},{"recording",true},{"save_destination",true},{"lens",true},{"peaking",true},{"star_autofocus",true},{"af_parameters",false},{"exposure_advice",true},{"sequence_details",true},{"sequence_camera",true},{"file_organization",false}};
    float focusScroll_{},pendingFocusScroll_{-1};
};
void applyTheme(bool light=false);
}
