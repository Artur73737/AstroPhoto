#include <astro/ConfigManager.hpp>
#include <nlohmann/json.hpp>
#include <fstream>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif
namespace astro {
using Json=nlohmann::json;
static Property parseProperty(const std::string& name) {
    for(auto p:{Property::Iso,Property::Aperture,Property::Shutter,Property::ExposureMode,Property::WhiteBalance,Property::ColorTemperature,Property::PictureStyle,Property::ImageQuality}) if(propertyName(p)==name) return p;
    throw std::invalid_argument("Proprieta sconosciuta: "+name);
}
static FrameType parseFrame(const std::string& name) {
    for(auto t:{FrameType::Light,FrameType::Dark,FrameType::Flat,FrameType::Bias}) if(frameName(t)==name) return t;
    throw std::invalid_argument("Tipo frame sconosciuto: "+name);
}
static void keys(const Json& j,std::initializer_list<const char*> allowed) {
    if(!j.is_object()) throw std::invalid_argument("Oggetto JSON richiesto");
    for(auto it=j.begin();it!=j.end();++it) if(std::none_of(allowed.begin(),allowed.end(),[&](const char* k){return it.key()==k;})) throw std::invalid_argument("Chiave sconosciuta: "+it.key());
}
static std::uint32_t code(const Json& j) {
    if(!j.is_number_unsigned() && !j.is_number_integer()) throw std::invalid_argument("Codice proprieta non intero");
    const auto v=j.get<std::int64_t>(); if(v<0||v>UINT32_MAX) throw std::invalid_argument("Codice proprieta fuori intervallo"); return static_cast<std::uint32_t>(v);
}
static int integer(const Json& j) {
    if(!j.is_number_integer()) throw std::invalid_argument("Valore intero richiesto");
    auto v=j.get<std::int64_t>(); if(v<INT_MIN||v>INT_MAX) throw std::invalid_argument("Intero fuori intervallo");return static_cast<int>(v);
}
Config ConfigManager::load(const std::filesystem::path& path) {
    std::ifstream stream(path); if(!stream) throw std::runtime_error("Impossibile leggere "+path.string());
    return fromJson({std::istreambuf_iterator<char>(stream),{}});
}
Config ConfigManager::fromJson(const std::string& text) {
    const auto j=Json::parse(text);
    keys(j,{"schema_version","output_directory","session_name","tick_ms","live_view_ms","download_timeout_seconds","busy_retries","retry_delay_ms","window_width","window_height","max_log_entries","reserve_bytes","sequence","property_labels","ui_scale","light_theme","single_capture","language","focus_peaking","star_peaking","peaking_threshold","save_destination","crosshair_enabled","crosshair_shape","star_autofocus","keep_alive_seconds","reconnect_attempts","reconnect_delay_ms"});
    if(integer(j.at("schema_version"))!=1) throw std::invalid_argument("Versione configurazione non supportata");
    Config c;c.outputDirectory=pathFromUtf8(j.at("output_directory").get<std::string>()); c.sessionName=j.at("session_name").get<std::string>();
    c.tickMs=integer(j.at("tick_ms"));c.liveViewMs=integer(j.at("live_view_ms"));c.downloadTimeoutSeconds=integer(j.at("download_timeout_seconds"));
    if(j.contains("keep_alive_seconds"))c.keepAliveSeconds=integer(j.at("keep_alive_seconds"));
    if(j.contains("reconnect_attempts"))c.reconnectAttempts=integer(j.at("reconnect_attempts"));
    if(j.contains("reconnect_delay_ms"))c.reconnectDelayMs=integer(j.at("reconnect_delay_ms"));
    c.busyRetries=integer(j.at("busy_retries"));c.retryDelayMs=integer(j.at("retry_delay_ms"));
    c.windowWidth=integer(j.at("window_width"));c.windowHeight=integer(j.at("window_height"));c.maxLogEntries=integer(j.at("max_log_entries"));
    c.uiScale=j.value("ui_scale",1.0f);c.lightTheme=j.value("light_theme",false);
    c.language=j.value("language",std::string("it"));c.focusPeaking=j.value("focus_peaking",false);c.starPeaking=j.value("star_peaking",false);c.peakingThreshold=j.value("peaking_threshold",35.0f);
    const auto destination=j.value("save_destination",std::string("pc_only"));
    if(destination!="pc_only"&&destination!="pc_and_camera")throw std::invalid_argument("Invalid save destination");
    c.saveDestination=destination=="pc_only"?SaveDestination::PcOnly:SaveDestination::PcAndCamera;
    c.crosshairEnabled=j.value("crosshair_enabled",true);c.crosshairShape=j.value("crosshair_shape",std::string("cross"));
    if(j.contains("star_autofocus")){
        const auto& f=j.at("star_autofocus");keys(f,{"step","positions","frames_per_point","min_stars","settle_ms","backlash","max_moves","timeout_seconds","detection_snr","match_radius","max_scatter","verification_tolerance","search_enabled","search_step","search_span","max_passes","max_stride","fit_min_r_squared","fit_max_uncertainty"});
        auto& o=c.starFocus;o.searchEnabled=f.value("search_enabled",o.searchEnabled);
        auto read=[&](const char* key,int& value){if(f.contains(key))value=integer(f.at(key));};
        read("search_step",o.searchStep);read("search_span",o.searchSpan);read("max_passes",o.maxPasses);read("max_stride",o.maxStride);
        read("step",o.step);read("positions",o.positions);read("frames_per_point",o.framesPerPoint);read("min_stars",o.minStars);read("settle_ms",o.settleMs);read("backlash",o.backlash);read("max_moves",o.maxMoves);read("timeout_seconds",o.timeoutSeconds);
        o.detectionSnr=f.value("detection_snr",o.detectionSnr);o.matchRadius=f.value("match_radius",o.matchRadius);o.maxScatter=f.value("max_scatter",o.maxScatter);o.verificationTolerance=f.value("verification_tolerance",o.verificationTolerance);o.fitMinRSquared=f.value("fit_min_r_squared",o.fitMinRSquared);o.fitMaxUncertainty=f.value("fit_max_uncertainty",o.fitMaxUncertainty);
    }
    if(!j.at("reserve_bytes").is_number_unsigned()) throw std::invalid_argument("reserve_bytes deve essere positivo");
    c.reserveBytes=j.at("reserve_bytes").get<std::uint64_t>();
    if(!j.at("sequence").is_array()) throw std::invalid_argument("sequence deve essere un array");
    auto parseItem=[&](const Json& row) {
        keys(row,{"type","count","exposure_seconds","delay_seconds","interval_seconds","bulb","settings"});
        SequenceItem i;i.type=parseFrame(row.at("type").get<std::string>());i.count=integer(row.at("count"));
        i.exposureSeconds=row.at("exposure_seconds").get<double>();i.delaySeconds=row.at("delay_seconds").get<double>();i.bulb=row.at("bulb").get<bool>();
        if(!row.at("settings").is_object()) throw std::invalid_argument("settings deve essere un oggetto");
        for(auto it=row.at("settings").begin();it!=row.at("settings").end();++it) i.settings[parseProperty(it.key())]=code(it.value());
        i.intervalSeconds=row.value("interval_seconds",0.0);return i;
    };
    for(const auto& row:j.at("sequence"))c.sequence.push_back(parseItem(row));
    if(j.contains("single_capture"))c.singleCapture=parseItem(j.at("single_capture"));
    if(j.contains("property_labels")) for(auto it=j.at("property_labels").begin();it!=j.at("property_labels").end();++it) {
        auto p=parseProperty(it.key()); if(!it.value().is_object()) throw std::invalid_argument("Etichette non valide");
        for(auto entry=it.value().begin();entry!=it.value().end();++entry) {
            std::size_t used{};auto value=std::stoull(entry.key(),&used,0);
            if(used!=entry.key().size()||value>UINT32_MAX) throw std::invalid_argument("Codice etichetta non valido");
            c.labels[p][static_cast<std::uint32_t>(value)]=entry.value().get<std::string>();
        }
    }
    validate(c);return c;
}
std::string ConfigManager::toJson(const Config& c) {
    auto utf8=c.outputDirectory.u8string();
    Json j={{"schema_version",1},{"output_directory",std::string(utf8.begin(),utf8.end())},{"session_name",c.sessionName},{"tick_ms",c.tickMs},{"live_view_ms",c.liveViewMs},{"download_timeout_seconds",c.downloadTimeoutSeconds},{"busy_retries",c.busyRetries},{"retry_delay_ms",c.retryDelayMs},{"window_width",c.windowWidth},{"window_height",c.windowHeight},{"max_log_entries",c.maxLogEntries},{"reserve_bytes",c.reserveBytes},{"sequence",Json::array()},{"property_labels",Json::object()}};
    auto itemJson=[](const SequenceItem& i) {
        Json settings=Json::object();for(const auto& [p,v]:i.settings) settings[propertyName(p)]=v;
        return Json{{"type",frameName(i.type)},{"count",i.count},{"exposure_seconds",i.exposureSeconds},{"delay_seconds",i.delaySeconds},{"interval_seconds",i.intervalSeconds},{"bulb",i.bulb},{"settings",settings}};
    };
    for(const auto& i:c.sequence)j["sequence"].push_back(itemJson(i));
    j["single_capture"]=itemJson(c.singleCapture);j["light_theme"]=c.lightTheme;
    j["ui_scale"]=c.uiScale;
    j["keep_alive_seconds"]=c.keepAliveSeconds;j["reconnect_attempts"]=c.reconnectAttempts;j["reconnect_delay_ms"]=c.reconnectDelayMs;
    j["language"]=c.language;j["focus_peaking"]=c.focusPeaking;j["star_peaking"]=c.starPeaking;j["peaking_threshold"]=c.peakingThreshold;
    j["save_destination"]=c.saveDestination==SaveDestination::PcOnly?"pc_only":"pc_and_camera";
    j["crosshair_enabled"]=c.crosshairEnabled;j["crosshair_shape"]=c.crosshairShape;
    const auto& f=c.starFocus;j["star_autofocus"]={{"search_enabled",f.searchEnabled},{"search_step",f.searchStep},{"search_span",f.searchSpan},{"max_passes",f.maxPasses},{"max_stride",f.maxStride},{"step",f.step},{"positions",f.positions},{"frames_per_point",f.framesPerPoint},{"min_stars",f.minStars},{"settle_ms",f.settleMs},{"backlash",f.backlash},{"max_moves",f.maxMoves},{"timeout_seconds",f.timeoutSeconds},{"detection_snr",f.detectionSnr},{"match_radius",f.matchRadius},{"max_scatter",f.maxScatter},{"verification_tolerance",f.verificationTolerance},{"fit_min_r_squared",f.fitMinRSquared},{"fit_max_uncertainty",f.fitMaxUncertainty}};
    for(const auto& [p,labels]:c.labels) for(const auto& [v,label]:labels) j["property_labels"][propertyName(p)][std::to_string(v)]=label;
    return j.dump(2);
}
void ConfigManager::save(const std::filesystem::path& path,const Config& c) {
    validate(c);
    const auto text=toJson(c);
    if(!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path());
    auto temporary=path;temporary+=".tmp";
    {std::ofstream out(temporary,std::ios::binary|std::ios::trunc);out<<text<<'\n';out.flush();if(!out) throw std::runtime_error("Errore scrittura configurazione");}
#ifdef _WIN32
    if(!MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) throw std::runtime_error("Errore sostituzione configurazione: "+std::to_string(GetLastError()));
#else
    std::filesystem::rename(temporary,path);
#endif
}
}
