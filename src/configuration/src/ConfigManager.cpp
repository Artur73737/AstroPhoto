#include <astro/ConfigManager.hpp>
#include <astro/CameraModule.hpp>
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
    keys(j,{"live_stack","schema_version","turbo_live_stream","camera_module","output_directory","session_name","tick_ms","live_view_ms","live_analysis_ms","lens_poll_ms","download_timeout_seconds","busy_retries","retry_delay_ms","window_width","window_height","max_log_entries","reserve_bytes","sequence","property_labels","ui_scale","light_theme","night_theme","single_capture","language","focus_peaking","star_peaking","peaking_threshold","save_destination","crosshair_enabled","crosshair_shape","star_autofocus","keep_alive_seconds","reconnect_attempts","reconnect_delay_ms"});
    if(integer(j.at("schema_version"))!=1) throw std::invalid_argument("Versione configurazione non supportata");
    Config c;c.outputDirectory=pathFromUtf8(j.at("output_directory").get<std::string>()); c.sessionName=j.at("session_name").get<std::string>();
    c.cameraModule=j.value("camera_module",std::string("auto"));camera_models::validateSelection(c.cameraModule);
    c.tickMs=integer(j.at("tick_ms"));c.liveViewMs=integer(j.at("live_view_ms"));c.downloadTimeoutSeconds=integer(j.at("download_timeout_seconds"));
    if(j.contains("live_analysis_ms"))c.liveAnalysisMs=integer(j.at("live_analysis_ms"));
    if(j.contains("lens_poll_ms"))c.lensPollMs=integer(j.at("lens_poll_ms"));
    if(j.contains("keep_alive_seconds"))c.keepAliveSeconds=integer(j.at("keep_alive_seconds"));
    if(j.contains("reconnect_attempts"))c.reconnectAttempts=integer(j.at("reconnect_attempts"));
    if(j.contains("reconnect_delay_ms"))c.reconnectDelayMs=integer(j.at("reconnect_delay_ms"));
    c.busyRetries=integer(j.at("busy_retries"));c.retryDelayMs=integer(j.at("retry_delay_ms"));
    c.windowWidth=integer(j.at("window_width"));c.windowHeight=integer(j.at("window_height"));c.maxLogEntries=integer(j.at("max_log_entries"));
    c.turboLiveStream=j.value("turbo_live_stream",false);c.uiScale=j.value("ui_scale",1.0f);c.lightTheme=j.value("light_theme",false);c.nightTheme=j.value("night_theme",false);
    c.language=j.value("language",std::string("it"));c.focusPeaking=j.value("focus_peaking",false);c.starPeaking=j.value("star_peaking",false);c.peakingThreshold=j.value("peaking_threshold",35.0f);
    const auto destination=j.value("save_destination",std::string("pc_only"));
    if(destination!="pc_only"&&destination!="pc_and_camera")throw std::invalid_argument("Invalid save destination");
    c.saveDestination=destination=="pc_only"?SaveDestination::PcOnly:SaveDestination::PcAndCamera;
    c.crosshairEnabled=j.value("crosshair_enabled",true);c.crosshairShape=j.value("crosshair_shape",std::string("cross"));
    if(j.contains("live_stack")) {
        const auto& f=j.at("live_stack");keys(f,{"frames","delay_seconds","alignment","detection_snr","minimum_stars","maximum_rms","rejection","sigma","memory_limit_mb","cosmetic_correction","horizontal_banding","banding_strength","banding_radius","banding_max_offset","bias_directory","dark_directory","flat_directory","dark_flat_directory","background_normalization","gradient_removal","affine_alignment","checkpoint_frames","source_directory","drizzle_scale","drizzle_pixfrac","winsorized_rejection","export_tiff","export_xisf","solver_executable","solver_database","solver_fov","solver_timeout_seconds","frame_type","from_sequence","auto_rebuild"});auto& o=c.liveStack;
        if(f.contains("frames"))o.frames=integer(f.at("frames"));if(f.contains("minimum_stars"))o.minimumStars=integer(f.at("minimum_stars"));if(f.contains("memory_limit_mb"))o.memoryLimitMb=integer(f.at("memory_limit_mb"));
        o.delaySeconds=f.value("delay_seconds",o.delaySeconds);o.alignment=f.value("alignment",o.alignment);o.detectionSnr=f.value("detection_snr",o.detectionSnr);o.maximumRms=f.value("maximum_rms",o.maximumRms);o.rejection=f.value("rejection",o.rejection);o.sigma=f.value("sigma",o.sigma);o.cosmeticCorrection=f.value("cosmetic_correction",o.cosmeticCorrection);o.horizontalBanding=f.value("horizontal_banding",o.horizontalBanding);o.bandingStrength=f.value("banding_strength",o.bandingStrength);o.bandingMaxOffset=f.value("banding_max_offset",o.bandingMaxOffset);if(f.contains("banding_radius"))o.bandingRadius=integer(f.at("banding_radius"));o.biasDirectory=f.value("bias_directory",o.biasDirectory);o.darkDirectory=f.value("dark_directory",o.darkDirectory);o.flatDirectory=f.value("flat_directory",o.flatDirectory);o.darkFlatDirectory=f.value("dark_flat_directory",o.darkFlatDirectory);o.backgroundNormalization=f.value("background_normalization",o.backgroundNormalization);o.gradientRemoval=f.value("gradient_removal",o.gradientRemoval);o.affineAlignment=f.value("affine_alignment",o.affineAlignment);o.sourceDirectory=f.value("source_directory",o.sourceDirectory);o.drizzleScale=f.value("drizzle_scale",o.drizzleScale);o.drizzlePixfrac=f.value("drizzle_pixfrac",o.drizzlePixfrac);o.winsorizedRejection=f.value("winsorized_rejection",o.winsorizedRejection);o.exportTiff=f.value("export_tiff",o.exportTiff);o.exportXisf=f.value("export_xisf",o.exportXisf);o.frameType=f.value("frame_type",o.frameType);o.fromSequence=f.value("from_sequence",o.fromSequence);o.autoRebuild=f.value("auto_rebuild",o.autoRebuild);o.solverExecutable=f.value("solver_executable",o.solverExecutable);o.solverDatabase=f.value("solver_database",o.solverDatabase);o.solverFov=f.value("solver_fov",o.solverFov);if(f.contains("solver_timeout_seconds"))o.solverTimeoutSeconds=integer(f.at("solver_timeout_seconds"));if(f.contains("checkpoint_frames"))o.checkpointFrames=integer(f.at("checkpoint_frames"));
    }
    if(j.contains("star_autofocus")){
        const auto& f=j.at("star_autofocus");keys(f,{"small_frames_per_point","small_settle_timeout_ms","backlash_probe_limit","acquisition_near_moves","acquisition_far_moves","small_response_sigma","small_stability_fraction","auto_backlash","allow_precision_warning","step","positions","frames_per_point","min_stars","settle_ms","backlash","max_moves","timeout_seconds","detection_snr","match_radius","max_scatter","verification_tolerance","search_enabled","search_step","search_span","max_passes","max_stride","fit_min_r_squared","fit_max_uncertainty","homing_max_moves","homing_stable_points","homing_probe_moves","homing_noise_factor","homing_min_change","homing_min_contrast"});
        auto& o=c.starFocus;o.searchEnabled=f.value("search_enabled",o.searchEnabled);
        auto read=[&](const char* key,int& value){if(f.contains(key))value=integer(f.at(key));};
        read("homing_max_moves",o.homingMaxMoves);read("homing_stable_points",o.homingStablePoints);read("homing_probe_moves",o.homingProbeMoves);
        o.homingNoiseFactor=f.value("homing_noise_factor",o.homingNoiseFactor);o.homingMinChange=f.value("homing_min_change",o.homingMinChange);o.homingMinContrast=f.value("homing_min_contrast",o.homingMinContrast);
        read("search_step",o.searchStep);read("search_span",o.searchSpan);read("max_passes",o.maxPasses);read("max_stride",o.maxStride);
        read("small_frames_per_point",o.smallFramesPerPoint);read("small_settle_timeout_ms",o.smallSettleTimeoutMs);read("backlash_probe_limit",o.backlashProbeLimit);read("acquisition_near_moves",o.acquisitionNearMoves);read("acquisition_far_moves",o.acquisitionFarMoves);if(f.contains("small_response_sigma"))o.smallResponseSigma=f.at("small_response_sigma").get<double>();if(f.contains("small_stability_fraction"))o.smallStabilityFraction=f.at("small_stability_fraction").get<double>();if(f.contains("auto_backlash"))o.autoBacklash=f.at("auto_backlash").get<bool>();if(f.contains("allow_precision_warning"))o.allowPrecisionWarning=f.at("allow_precision_warning").get<bool>();
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
    camera_models::validateSelection(c.cameraModule);
    auto utf8=c.outputDirectory.u8string();
    Json j={{"schema_version",1},{"output_directory",std::string(utf8.begin(),utf8.end())},{"session_name",c.sessionName},{"tick_ms",c.tickMs},{"live_view_ms",c.liveViewMs},{"download_timeout_seconds",c.downloadTimeoutSeconds},{"busy_retries",c.busyRetries},{"retry_delay_ms",c.retryDelayMs},{"window_width",c.windowWidth},{"window_height",c.windowHeight},{"max_log_entries",c.maxLogEntries},{"reserve_bytes",c.reserveBytes},{"sequence",Json::array()},{"property_labels",Json::object()}};
    auto itemJson=[](const SequenceItem& i) {
        Json settings=Json::object();for(const auto& [p,v]:i.settings) settings[propertyName(p)]=v;
        return Json{{"type",frameName(i.type)},{"count",i.count},{"exposure_seconds",i.exposureSeconds},{"delay_seconds",i.delaySeconds},{"interval_seconds",i.intervalSeconds},{"bulb",i.bulb},{"settings",settings}};
    };
    for(const auto& i:c.sequence)j["sequence"].push_back(itemJson(i));
    j["single_capture"]=itemJson(c.singleCapture);j["light_theme"]=c.lightTheme;j["night_theme"]=c.nightTheme;
    j["turbo_live_stream"]=c.turboLiveStream;j["ui_scale"]=c.uiScale;j["camera_module"]=c.cameraModule;
    j["keep_alive_seconds"]=c.keepAliveSeconds;j["reconnect_attempts"]=c.reconnectAttempts;j["reconnect_delay_ms"]=c.reconnectDelayMs;
    j["live_analysis_ms"]=c.liveAnalysisMs;j["lens_poll_ms"]=c.lensPollMs;
    j["language"]=c.language;j["focus_peaking"]=c.focusPeaking;j["star_peaking"]=c.starPeaking;j["peaking_threshold"]=c.peakingThreshold;
    j["save_destination"]=c.saveDestination==SaveDestination::PcOnly?"pc_only":"pc_and_camera";
    j["crosshair_enabled"]=c.crosshairEnabled;j["crosshair_shape"]=c.crosshairShape;
    const auto& o=c.liveStack;j["live_stack"]={{"frames",o.frames},{"delay_seconds",o.delaySeconds},{"alignment",o.alignment},{"detection_snr",o.detectionSnr},{"minimum_stars",o.minimumStars},{"maximum_rms",o.maximumRms},{"rejection",o.rejection},{"sigma",o.sigma},{"memory_limit_mb",o.memoryLimitMb},{"cosmetic_correction",o.cosmeticCorrection},{"horizontal_banding",o.horizontalBanding},{"banding_strength",o.bandingStrength},{"banding_radius",o.bandingRadius},{"banding_max_offset",o.bandingMaxOffset},{"bias_directory",o.biasDirectory},{"dark_directory",o.darkDirectory},{"flat_directory",o.flatDirectory},{"dark_flat_directory",o.darkFlatDirectory},{"background_normalization",o.backgroundNormalization},{"gradient_removal",o.gradientRemoval},{"affine_alignment",o.affineAlignment},{"checkpoint_frames",o.checkpointFrames},{"source_directory",o.sourceDirectory},{"drizzle_scale",o.drizzleScale},{"drizzle_pixfrac",o.drizzlePixfrac},{"winsorized_rejection",o.winsorizedRejection},{"export_tiff",o.exportTiff},{"export_xisf",o.exportXisf},{"solver_executable",o.solverExecutable},{"solver_database",o.solverDatabase},{"solver_fov",o.solverFov},{"solver_timeout_seconds",o.solverTimeoutSeconds},{"frame_type",o.frameType},{"from_sequence",o.fromSequence},{"auto_rebuild",o.autoRebuild}};
    const auto& f=c.starFocus;j["star_autofocus"]={{"small_frames_per_point",f.smallFramesPerPoint},{"small_settle_timeout_ms",f.smallSettleTimeoutMs},{"backlash_probe_limit",f.backlashProbeLimit},{"acquisition_near_moves",f.acquisitionNearMoves},{"acquisition_far_moves",f.acquisitionFarMoves},{"small_response_sigma",f.smallResponseSigma},{"small_stability_fraction",f.smallStabilityFraction},{"auto_backlash",f.autoBacklash},{"allow_precision_warning",f.allowPrecisionWarning},{"homing_max_moves",f.homingMaxMoves},{"homing_stable_points",f.homingStablePoints},{"homing_probe_moves",f.homingProbeMoves},{"homing_noise_factor",f.homingNoiseFactor},{"homing_min_change",f.homingMinChange},{"homing_min_contrast",f.homingMinContrast},{"search_enabled",f.searchEnabled},{"search_step",f.searchStep},{"search_span",f.searchSpan},{"max_passes",f.maxPasses},{"max_stride",f.maxStride},{"step",f.step},{"positions",f.positions},{"frames_per_point",f.framesPerPoint},{"min_stars",f.minStars},{"settle_ms",f.settleMs},{"backlash",f.backlash},{"max_moves",f.maxMoves},{"timeout_seconds",f.timeoutSeconds},{"detection_snr",f.detectionSnr},{"match_radius",f.matchRadius},{"max_scatter",f.maxScatter},{"verification_tolerance",f.verificationTolerance},{"fit_min_r_squared",f.fitMinRSquared},{"fit_max_uncertainty",f.fitMaxUncertainty}};
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
