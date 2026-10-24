#include "ControlApi.hpp"
#include <astro/ConfigManager.hpp>
#include <imgui.h>
#include <imgui_internal.h>
#include <cmath>
#include <thread>
namespace astro {
using J=ControlJson;
namespace {
const std::map<std::string,Action> actions{
    {"connect",Action::Connect},{"disconnect",Action::Disconnect},{"refresh",Action::Refresh},
    {"set_property",Action::SetProperty},{"sequence_start",Action::Start},{"capture_single",Action::CaptureSingle},
    {"sequence_pause",Action::Pause},{"sequence_resume",Action::Resume},{"stop",Action::Stop},
    {"live_view",Action::LiveView},{"lens_move",Action::Focus},{"zoom",Action::Zoom},
    {"save_config",Action::SaveConfig},{"autofocus_start",Action::StartStarFocus},{"autofocus_stop",Action::StopStarFocus}};
const char* panelIds[]{"Toolbar","Connection","Camera","LiveView","Sequence","Output","Log"};
std::string utf8(const std::filesystem::path& p){auto u=p.u8string();return {u.begin(),u.end()};}
void keys(const J& j,std::initializer_list<const char*> names){
    if(!j.is_object())throw std::invalid_argument("Expected a JSON object");
    for(auto it=j.begin();it!=j.end();++it)if(std::none_of(names.begin(),names.end(),[&](const char* n){return it.key()==n;}))throw std::invalid_argument("Unknown field: "+it.key());
}
int integer(const J& j){if(!j.is_number_integer())throw std::invalid_argument("Expected integer");const auto n=j.get<std::int64_t>();if(n<INT_MIN||n>INT_MAX)throw std::invalid_argument("Integer out of range");return static_cast<int>(n);}
Property property(const std::string& name){for(auto p:{Property::Iso,Property::Aperture,Property::Shutter,Property::ExposureMode,Property::WhiteBalance,Property::ColorTemperature,Property::PictureStyle,Property::ImageQuality})if(propertyName(p)==name)return p;throw std::invalid_argument("Unknown camera property");}
J uiState(const ui::ControlState& s){
    J panels=J::object();for(int i=0;i<6;++i)panels[panelIds[i+1]]=s.panels[i];
    return {{"view",s.viewLive?"live":"capture"},{"histogram",s.histogram},{"select_focus",s.selectFocus},{"focus_step",s.focusStep},
        {"selected_row",s.selectedRow},{"log_level",s.logLevel},{"log_filter",s.logFilter},{"auto_scroll",s.autoScroll},{"panels",panels},
        {"focus_target",{{"enabled",s.target.enabled},{"u",s.target.u},{"v",s.target.v},{"revision",s.target.revision}}},{"error",s.error},{"sections",s.sections},{"focus_scroll_y",s.focusScroll}};
}
J snapshot(const Snapshot& s){
    J props=J::array();for(const auto& p:s.properties){J choices=J::array();for(const auto& c:p.choices)choices.push_back({{"value",c.value},{"label",c.label}});props.push_back({{"id",propertyName(p.id)},{"value",p.value},{"label",p.currentLabel},{"writable",p.writable},{"choices",choices}});}
    J stars=J::array();for(const auto& x:s.stars.stars)stars.push_back({{"x",x.x},{"y",x.y},{"hfd",x.hfd},{"flux",x.flux},{"snr",x.snr},{"roundness",x.roundness},{"peak",x.peak},{"background",x.background}});
    const auto& f=s.autofocus;J curve=J::array();for(const auto& p:f.curve)curve.push_back({{"position",p.position},{"hfd",p.hfd},{"scatter",p.scatter}});
    const char* phases[]{"idle","baseline","searching","reanchoring","moving","sampling","returning","verifying","complete","failed","cancelled"};
    const char* lenses[]{"unknown","no_lens","manual_focus","ready","unsupported"};
    J files=J::array();for(const auto& p:s.lastCapture.files)files.push_back(utf8(p));
    return {{"backend_available",s.backendAvailable},{"backend_status",s.backendStatus},{"connected",s.connected},{"model",s.model},{"live_view",s.liveView},{"reconnecting",s.reconnecting},
        {"properties",props},{"lens",{{"state",lenses[static_cast<int>(s.lensFocus.state)]},{"detail",s.lensFocus.detail},{"manual_exposure",s.manualFocusExposure}}},
        {"sequence",{{"state",stateName(s.progress.state)},{"row",s.progress.row},{"completed",s.progress.completed},{"total",s.progress.total},{"pause_requested",s.progress.pauseRequested},{"detail",s.progress.detail}}},
        {"frame",{{"revision",s.frame.revision},{"jpeg_bytes",s.frame.jpeg.size()},{"zoom",s.frame.zoom},{"coordinate_width",s.frame.coordinateWidth},{"coordinate_height",s.frame.coordinateHeight},{"image_x",s.frame.imageX},{"image_y",s.frame.imageY},{"zoom_rect",{s.frame.zoomRect.x,s.frame.zoomRect.y,s.frame.zoomRect.width,s.frame.zoomRect.height}}}},
        {"last_capture",{{"revision",s.lastCapture.revision},{"type",frameName(s.lastCapture.type)},{"files",files}}},
        {"stars",{{"width",s.stars.width},{"height",s.stars.height},{"saturated",s.stars.saturated},{"rejected",s.stars.rejected},{"broad_candidates",s.stars.acquisitionCandidates},{"background",s.stars.background},{"noise",s.stars.noise},{"clipped_fraction",s.stars.clippedFraction},{"samples",stars}}},
        {"autofocus",{{"phase",phases[static_cast<int>(f.phase)]},{"active",f.active()},{"position",f.position},{"moves",f.moves},{"frames",f.frames},{"stars",f.stars},{"search_moves",f.searchMoves},{"pass",f.pass},{"stride",f.stride},{"motor_step",f.motorStep},{"missing_points",f.missingPoints},{"paired_stars",f.pairedStars},{"bracketed",f.bracketed},{"restored",f.restored},{"stage_best",f.stageBest},{"baseline",f.baseline},{"current",f.current},{"best",f.best},{"verified",f.verified},{"curve_r2",f.curveR2},{"fit_valid",f.fitValid},{"fit_minimum",f.fitMinimum},{"fit_uncertainty",f.fitUncertainty},{"fit_parameters",f.fitParameters},{"detail",f.detail},{"curve",curve}}}};
}
}
ControlJson previewTelemetry(const Histogram& h,const FocusReading& f){return {{"histogram",{{"bins",h.bins},{"pixels",h.pixels}}},{"focus",{{"valid",f.valid},{"bracketed",f.bracketed},{"near_best",f.nearBest},{"score",f.score},{"best",f.best},{"samples",f.samples},{"roi",{f.x,f.y,f.width,f.height}},{"peaking_measured",f.peakingMeasured},{"peaking_fraction",f.peakingFraction},{"peaking_pixels",f.peakingPixels}}}};}
ControlJson ControlApi::call(const J& request,const std::function<J(const J&)>& image) {
    keys(request,{"method","params"});const auto method=request.at("method").get<std::string>();const auto p=request.value("params",J::object());const auto s=mailbox_.snapshot();
    if(method=="state")return {{"ok",true},{"state",snapshot(s)},{"ui",uiState(ui_.controlState())},{"ui_frame",ImGui::GetFrameCount()},{"core_tick",s.coreTick},{"threads",{{"ui",std::hash<std::thread::id>{}(std::this_thread::get_id())},{"core",s.coreThreadId}}}};
    if(method=="config")return {{"ok",true},{"config",J::parse(ConfigManager::toJson(ui_.controlConfig()))}};
    if(method=="config_patch"){
        if(s.autofocus.active()||s.progress.state==SequenceState::Running||s.progress.state==SequenceState::Paused||s.progress.state==SequenceState::Stopping)throw std::logic_error("Stop capture/autofocus before changing configuration via MCP");
        if(!p.is_object())throw std::invalid_argument("Expected configuration patch object");
        auto j=J::parse(ConfigManager::toJson(ui_.controlConfig()));j.merge_patch(p);auto c=ConfigManager::fromJson(j.dump());
        if(c.starFocus.step!=1||c.starFocus.searchStep!=3||c.starFocus.maxPasses<3)throw std::invalid_argument("Autofocus requires large -> medium -> small (search_step=3, step=1, max_passes>=3)");
        ui_.controlConfig(c);return {{"ok",true},{"config",j},{"persisted",false},{"note","Use save_config to persist. Worker timing/retry/connection settings and initial window dimensions take effect after app restart."}};
    }
    if(method=="ui_patch"){
        keys(p,{"view","histogram","select_focus","focus_step","selected_row","log_level","log_filter","auto_scroll","panels","focus_target","reset_focus","sections","focus_scroll_y"});auto u=ui_.controlState();
        if(p.contains("sections")){const auto entries=p.at("sections").get<std::map<std::string,bool>>();for(const auto& [id,open]:entries)u.sections[id]=open;}
        if(p.contains("focus_scroll_y"))u.focusScroll=p.at("focus_scroll_y").get<float>();
        if(p.contains("view")){auto v=p.at("view").get<std::string>();if(v!="live"&&v!="capture")throw std::invalid_argument("view must be live or capture");u.viewLive=v=="live";}
        if(p.contains("histogram"))u.histogram=p.at("histogram").get<bool>();
        if(p.contains("select_focus")){u.selectFocus=p.at("select_focus").get<bool>();u.target.enabled=false;++u.target.revision;}
        if(p.contains("focus_step"))u.focusStep=integer(p.at("focus_step"));
        if(p.contains("selected_row"))u.selectedRow=integer(p.at("selected_row"));
        if(p.contains("log_level"))u.logLevel=integer(p.at("log_level"));
        if(p.contains("log_filter"))u.logFilter=p.at("log_filter").get<std::string>();
        if(p.contains("auto_scroll"))u.autoScroll=p.at("auto_scroll").get<bool>();
        if(p.contains("panels")){const auto& panels=p.at("panels");keys(panels,{"Connection","Camera","LiveView","Sequence","Output","Log"});for(int i=0;i<6;++i)if(panels.contains(panelIds[i+1]))u.panels[i]=panels.at(panelIds[i+1]).get<bool>();}
        if(p.contains("focus_target")){const auto& t=p.at("focus_target");keys(t,{"enabled","u","v"});u.target.enabled=t.value("enabled",true);u.target.u=t.value("u",u.target.u);u.target.v=t.value("v",u.target.v);u.selectFocus=u.target.enabled;++u.target.revision;}
        if(p.value("reset_focus",false))++u.target.revision;
        ui_.controlState(u);return {{"ok",true},{"ui",uiState(u)}};
    }
    if(method=="logs"){keys(p,{"limit","level","contains"});int limit=p.contains("limit")?integer(p.at("limit")):100;if(limit<1||limit>1000)throw std::invalid_argument("limit must be 1..1000");J logs=J::array();auto level=p.value("level",std::string{}),text=p.value("contains",std::string{});for(auto it=s.logs.rbegin();it!=s.logs.rend()&&logs.size()<static_cast<std::size_t>(limit);++it)if((level.empty()||it->level==level)&&(text.empty()||it->message.find(text)!=std::string::npos))logs.push_back({{"time",it->time},{"level",it->level},{"message",it->message}});std::reverse(logs.begin(),logs.end());return {{"ok",true},{"logs",logs}};}
    if(method=="command_status"){keys(p,{"id"});auto result=mailbox_.result(p.at("id").get<std::uint64_t>());if(!result)throw std::invalid_argument("Command not found (history limited to 512 commands in this process)");return {{"ok",true},{"id",result->id},{"status",!result->finished?"queued":result->error.empty()?"completed":"failed"},{"error",result->error},{"note","completed acknowledges the command; inspect sequence/autofocus state for long-running operation outcome"}};}
    if(method=="command"){
        keys(p,{"action","value","property"});auto name=p.at("action").get<std::string>();auto it=actions.find(name);if(it==actions.end())throw std::invalid_argument("Unknown action");
        Command c;c.action=it->second;c.config=ui_.controlConfig();
        if(c.action==Action::SetProperty){c.property=property(p.at("property").get<std::string>());const auto& value=p.at("value");if(!value.is_number_integer()||value.get<std::int64_t>()<0||value.get<std::uint64_t>()>UINT32_MAX)throw std::invalid_argument("Invalid property code");c.value=value.get<std::uint32_t>();}
        if(c.action==Action::LiveView)c.value=p.at("value").get<bool>()?1:0;
        if(c.action==Action::Zoom){const auto ratio=integer(p.at("value"));if(ratio!=1&&ratio!=5&&ratio!=10)throw std::invalid_argument("Zoom must be 1, 5 or 10");c.value=ratio;}
        if(c.action==Action::Focus){const int step=integer(p.at("value"));if(step==0||step< -3||step>3)throw std::invalid_argument("Lens move must be -3,-2,-1,1,2,3");c.value=step+3;}
        if(c.action==Action::Start||c.action==Action::CaptureSingle){validate(c.config);for(const auto& prop:s.properties)if(prop.id==Property::Iso||prop.id==Property::Aperture||prop.id==Property::Shutter){if(c.action==Action::CaptureSingle)c.config.singleCapture.settings[prop.id]=prop.value;else for(auto& row:c.config.sequence)if(!row.settings.contains(prop.id))row.settings[prop.id]=prop.value;}}
        auto id=mailbox_.submit(std::move(c));auto u=ui_.controlState();if(name=="live_view"||name=="autofocus_start")u.viewLive=true;if(name=="capture_single")u.viewLive=false;ui_.controlState(u);
        return {{"ok",true},{"id",id},{"status","queued"}};
    }
    if(method=="layout"){
        keys(p,{"reset","panel","x","y","width","height","collapsed","scroll_y"});
        if(p.value("reset",false))ui_.resetLayout();
        if(p.contains("panel")){
            const auto id=p.at("panel").get<std::string>();if(std::none_of(std::begin(panelIds),std::end(panelIds),[&](const char* n){return id==n;}))throw std::invalid_argument("Unknown panel");
            auto* w=ImGui::FindWindowByName(("###"+id).c_str());if(!w)throw std::invalid_argument("Panel has not been rendered");
            float x=p.value("x",w->Pos.x),y=p.value("y",w->Pos.y),width=p.value("width",w->Size.x),height=p.value("height",w->Size.y),scroll=p.value("scroll_y",w->Scroll.y);
            if(!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(width)||!std::isfinite(height)||!std::isfinite(scroll)||std::abs(x)>100000||std::abs(y)>100000||width<32||height<32||width>16384||height>16384||scroll<0)throw std::invalid_argument("Invalid panel geometry");
            ImGui::SetWindowPos(w,{x,y});ImGui::SetWindowSize(w,{width,height});if(p.contains("collapsed"))ImGui::SetWindowCollapsed(w,p.at("collapsed").get<bool>());if(p.contains("scroll_y"))ImGui::SetScrollY(w,scroll);
        }
        J panels=J::array();for(const auto* id:panelIds)if(auto* w=ImGui::FindWindowByName((std::string("###")+id).c_str()))panels.push_back({{"id",id},{"visible",w->Active&&!w->Hidden},{"x",w->Pos.x},{"y",w->Pos.y},{"width",w->Size.x},{"height",w->Size.y},{"collapsed",w->Collapsed},{"scroll_y",w->Scroll.y},{"dpi",w->Viewport?w->Viewport->DpiScale:1.f}});
        J monitors=J::array();for(const auto& m:ImGui::GetPlatformIO().Monitors)monitors.push_back({{"x",m.WorkPos.x},{"y",m.WorkPos.y},{"width",m.WorkSize.x},{"height",m.WorkSize.y},{"dpi",m.DpiScale}});
        return {{"ok",true},{"panels",panels},{"monitors",monitors},{"note","The normal layout manager keeps panels inside work areas and resolves overlaps on the next frame."}};
    }
    if(method=="window"){keys(p,{"action"});const auto a=p.at("action").get<std::string>();if(a=="close"){if(s.autofocus.active()||s.progress.state==SequenceState::Running||s.progress.state==SequenceState::Paused||s.progress.state==SequenceState::Stopping)throw std::logic_error("Stop active acquisition before closing");ui_.controlExit();}else if(a=="minimize")ui_.controlWindow(1);else if(a=="restore")ui_.controlWindow(2);else throw std::invalid_argument("Unknown window action");return {{"ok",true}};}
    if(method=="image"||method=="analysis")return image(request);
    throw std::invalid_argument("Unknown control method: "+method);
}
}
