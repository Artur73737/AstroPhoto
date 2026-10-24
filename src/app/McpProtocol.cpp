#include "McpProtocol.hpp"
#include <astro/ConfigManager.hpp>
#include <cmath>
#include <thread>
namespace astro::mcp {
namespace {
Json object(Json properties=Json::object(),Json required=Json::array()){return {{"type","object"},{"properties",std::move(properties)},{"required",std::move(required)},{"additionalProperties",false}};}
Json type(const char* name){return {{"type",name}};}
Json enumeration(std::initializer_list<const char*> values){return {{"type","string"},{"enum",values}};}
Json range(const char* name,double minimum,double maximum){return {{"type",name},{"minimum",minimum},{"maximum",maximum}};}
Json infer(const Json& value){
    if(value.is_object()){Json properties=Json::object();for(auto it=value.begin();it!=value.end();++it)properties[it.key()]=infer(it.value());return object(properties);}
    if(value.is_array())return {{"type","array"}};
    return type(value.is_boolean()?"boolean":value.is_number_integer()?"integer":value.is_number()?"number":"string");
}
Json settingsSchema(){
    auto c=ConfigManager::toJson(Config{});auto schema=infer(Json::parse(c));auto& p=schema["properties"];p.erase("schema_version");
    p["language"]=enumeration({"it","en"});p["save_destination"]=enumeration({"pc_only","pc_and_camera"});p["crosshair_shape"]=enumeration({"cross","gap_cross","circle","target","thirds"});p["ui_scale"]=range("number",.75,2);p["peaking_threshold"]=range("number",10,100);
    p["property_labels"]={{"type","object"},{"additionalProperties",{{"type","object"},{"additionalProperties",type("string")}}}};
    auto& single=p["single_capture"];single["properties"]["type"]=enumeration({"Light","Dark","Flat","Bias"});single["properties"]["settings"]={{"type","object"},{"additionalProperties",range("integer",0,UINT32_MAX)}};
    auto row=single;row["required"]={"type","count","exposure_seconds","delay_seconds","interval_seconds","bulb","settings"};
    p["sequence"]={{"type","array"},{"items",row}};
    auto& af=p["star_autofocus"]["properties"];
    af["step"]={{"type","integer"},{"enum",{1}}};af["search_step"]={{"type","integer"},{"enum",{3}}};
    for(auto [key,lo,hi]:std::initializer_list<std::tuple<const char*,int,int>>{{"search_span",2,40},{"max_passes",3,8},{"max_stride",1,16},{"positions",7,21},{"frames_per_point",3,15},{"min_stars",3,20},{"settle_ms",200,5000},{"backlash",1,10},{"max_moves",20,200},{"timeout_seconds",30,600}})af[key]=range("integer",lo,hi);
    for(auto [key,lo,hi]:std::initializer_list<std::tuple<const char*,double,double>>{{"detection_snr",4,20},{"match_radius",2,30},{"max_scatter",.02,.25},{"verification_tolerance",.02,.2},{"fit_min_r_squared",.5,.9999},{"fit_max_uncertainty",.1,3}})af[key]=range("number",lo,hi);
    return schema;
}
Json uiSchema(){return object({
    {"view",enumeration({"live","capture"})},{"histogram",type("boolean")},{"select_focus",type("boolean")},{"reset_focus",type("boolean")},
    {"focus_step",range("integer",1,3)},{"selected_row",range("integer",-1,INT_MAX)},{"log_level",range("integer",0,3)},{"log_filter",type("string")},{"auto_scroll",type("boolean")},
    {"panels",object({{"Connection",type("boolean")},{"Camera",type("boolean")},{"LiveView",type("boolean")},{"Sequence",type("boolean")},{"Output",type("boolean")},{"Log",type("boolean")}})},
    {"sections",{{"type","object"},{"additionalProperties",type("boolean")}}},{"focus_scroll_y",range("number",0,100000)},
    {"focus_target",object({{"enabled",type("boolean")},{"u",range("number",0,1)},{"v",range("number",0,1)}})}});}
Json catalog(){
    Json tools=Json::array();auto add=[&](const char* name,const char* description,Json schema,bool read=true){tools.push_back({{"name",name},{"description",description},{"inputSchema",std::move(schema)},{"annotations",{{"readOnlyHint",read},{"destructiveHint",false},{"openWorldHint",false}}}});};
    const auto empty=object();const auto panels=enumeration({"Toolbar","Connection","Camera","LiveView","Sequence","Output","Log"});
    add("app_status","Check the real application, without launching or connecting a camera.",empty);
    add("app_launch","Report this already-running UI host. MCP is internal: never starts a second application/camera session.",empty,false);
    add("get_state","Full camera state, properties and choices, sequence progress, star samples, all AF phases/curves/three stages and UI state.",empty);
    add("get_settings","Read every current configuration value, including single capture, sequence rows, all AF and presentation options.",empty);
    add("get_control_schema","Discover all settings/UI schemas and restart semantics.",empty);
    add("update_settings","Atomically validate and merge the real UI configuration. Sequence arrays replace all rows. Save separately. Stop acquisition before editing.",object({{"patch",settingsSchema()}},{"patch"}),false);
    add("save_settings","Persist current UI configuration to user.json. Check command_status for disk write errors.",empty,false);
    add("update_ui","Control preview, point focus coordinates/reset, peaking histogram visibility, lens step, panels, expandable sections and log filters.",object({{"patch",uiSchema()}},{"patch"}),false);
    add("camera_connection","Connect, disconnect, or refresh the physical Canon. Command returns an ID, not a completion claim.",object({{"action",enumeration({"connect","disconnect","refresh"})}},{"action"}),false);
    add("camera_properties","Read camera-reported supported values and readable labels, writable flags and lens AF status.",empty);
    add("camera_set_property","Set one camera property by an exact reported label OR native integer value. Get choices first.",object({{"property",enumeration({"ISO","Av","Tv","AE","WB","Kelvin","PictureStyle","ImageQuality"})},{"value",range("integer",0,UINT32_MAX)},{"label",type("string")}},{"property"}),false);
    add("live_view","Enable/disable actual camera Live View. Stopping cancels AF. Also selects the live preview.",object({{"enabled",type("boolean")}},{"enabled"}),false);
    add("lens_move","ONE physical lens move: negative near, positive infinity; 1 small, 2 medium, 3 large. Requires AF-ready lens/Live View. Not degrees.",object({{"step",{{"type","integer"},{"enum",{-3,-2,-1,1,2,3}}}}},{"step"}),false);
    add("live_zoom","Set physical 1x/5x/10x live preview magnification through the normal camera worker.",object({{"ratio",{{"type","integer"},{"enum",{1,5,10}}}}},{"ratio"}),false);
    add("capture_single","Take one real photo using single_capture and current camera ISO/Av/Tv, matching the UI. Follow state until download completes.",empty,false);
    add("sequence_control","Start/pause/resume/stop the configured real sequence. Stop also cancels AF; pause waits for a safe boundary.",object({{"action",enumeration({"start","pause","resume","stop"})}},{"action"}),false);
    add("autofocus_control","Start/cancel stellar autofocus with current parameters. Requires live view, AF lens, M, fixed ISO, non-Bulb Tv. Large -> medium -> small.",object({{"action",enumeration({"start","stop"})}},{"action"}),false);
    add("command_status","Read queued/completed/failed command acknowledgement. completed does not mean exposure/AF finished: inspect get_state.",object({{"id",range("integer",1,9007199254740991.)}},{"id"}));
    add("get_logs","Real application log tail (chronological). Filter by level/text.",object({{"limit",range("integer",1,1000)},{"level",enumeration({"","Info","Warning","Error"})},{"contains",type("string")}}));
    add("get_analysis","Get unmodified RGB/Y histogram bins and measured point-focus/peaking score, validity and bracket flags.",object({{"source",enumeration({"live","capture"})}}));
    add("get_image","Real MCP image: rendered UI panel including all overlays, processed live texture, original camera JPEG or last captured preview. No desktop screenshot.",object({{"source",enumeration({"panel","live","raw_live","capture"})},{"panel",panels}}));
    add("get_layout","Read all panel positions, sizes, visibility, collapse/scroll and monitor work areas/DPI.",empty);
    add("set_layout","Arrange/collapse/scroll a panel or reset primary layout. Normal UI resolves overlaps/out-of-bounds on next frame.",object({{"panel",panels},{"x",range("number",-100000,100000)},{"y",range("number",-100000,100000)},{"width",range("number",32,16384)},{"height",range("number",32,16384)},{"collapsed",type("boolean")},{"scroll_y",range("number",0,100000)},{"reset",type("boolean")}}),false);
    add("app_window","Minimize/restore/close the UI group. Close refuses active acquisition. Save settings first if desired.",object({{"action",enumeration({"minimize","restore","close"})}},{"action"}),false);
    return tools;
}
void validateSchema(const Json& value,const Json& schema,const std::string& path="arguments"){
    const auto t=schema.value("type",std::string{});
    const bool valid=t=="object"?value.is_object():t=="array"?value.is_array():t=="string"?value.is_string():t=="boolean"?value.is_boolean():t=="integer"?value.is_number_integer():t=="number"?value.is_number():true;
    if(!valid)throw std::invalid_argument(path+": expected "+t);
    if(schema.contains("enum")&&std::find(schema["enum"].begin(),schema["enum"].end(),value)==schema["enum"].end())throw std::invalid_argument(path+": unsupported value");
    if(value.is_number()){double n=value.get<double>();if(!std::isfinite(n)||(schema.contains("minimum")&&n<schema["minimum"].get<double>())||(schema.contains("maximum")&&n>schema["maximum"].get<double>()))throw std::invalid_argument(path+": out of range");}
    if(value.is_object()){
        const auto properties=schema.value("properties",Json::object());for(const auto& key:schema.value("required",Json::array()))if(!value.contains(key.get<std::string>()))throw std::invalid_argument(path+": missing "+key.get<std::string>());
        for(auto it=value.begin();it!=value.end();++it){if(properties.contains(it.key()))validateSchema(it.value(),properties[it.key()],path+"."+it.key());else if(schema.contains("additionalProperties")&&schema["additionalProperties"].is_object())validateSchema(it.value(),schema["additionalProperties"],path+"."+it.key());else if(schema.value("additionalProperties",Json(true))==false)throw std::invalid_argument(path+": unknown field "+it.key());}
    }
    if(value.is_array()&&schema.contains("items"))for(const auto& item:value)validateSchema(item,schema["items"],path+"[]");
}
Json content(Json data){return {{"content",Json::array({{{"type","text"},{"text",data.dump()}}})},{"structuredContent",std::move(data)},{"isError",false}};}
}
Json error(const Json& id,int code,const std::string& message){return {{"jsonrpc","2.0"},{"id",id},{"error",{{"code",code},{"message",message}}}};}
Protocol::Protocol(Control control,std::function<Json()> launch):control_(std::move(control)),launch_(std::move(launch)){}
Json Protocol::tools() const {static const auto result=catalog();return result;}
std::optional<Json> Protocol::dispatch(const Json& request){
    if(!request.is_object()||request.value("jsonrpc",Json{})!="2.0"||!request.contains("method")||!request["method"].is_string())return error(nullptr,-32600,"Invalid JSON-RPC request");
    const bool notification=!request.contains("id");const auto id=request.value("id",Json{});if(!notification&&!id.is_string()&&!id.is_number_integer())return error(nullptr,-32600,"Invalid request ID");
    const auto method=request["method"].get<std::string>();
    if(notification)return {}; // No response to initialized/cancelled/progress notifications.
    try {
        const auto p=request.value("params",Json::object());if(!p.is_object())return error(id,-32602,"params must be an object");Json result;
        if(method=="initialize"){
            auto version=p.value("protocolVersion",std::string("2025-11-25"));if(version!="2025-11-25"&&version!="2025-06-18"&&version!="2025-03-26"&&version!="2024-11-05")version="2025-11-25";
            result={{"protocolVersion",version},{"capabilities",{{"tools",{{"listChanged",false}}}}},{"serverInfo",{{"name","AstroPhoto"},{"version","0.4.0"}}},{"instructions","Native C++ control inside the real AstroPhoto UI process, using one selected transport. Inspect state before hardware actions. Commands return IDs; poll command_status then get_state for operation results. Never repeat a timed-out shutter/lens command blindly. AF accuracy still requires real-camera validation."}};
        }else if(method=="ping")result=Json::object();
        else if(method=="tools/list")result={{"tools",tools()}};
        else if(method=="tools/call"){
            const auto name=p.at("name").get<std::string>();const auto args=p.value("arguments",Json::object());auto all=tools();auto it=std::find_if(all.begin(),all.end(),[&](const Json& t){return t["name"]==name;});
            if(it==all.end())return error(id,-32602,"Unknown tool: "+name);
            try{validateSchema(args,(*it)["inputSchema"]);result=invoke(name,args);}
            catch(const std::exception& e){result={{"content",Json::array({{{"type","text"},{"text",e.what()}}})},{"isError",true}};}
        }else return error(id,-32601,"Method not found");
        return Json{{"jsonrpc","2.0"},{"id",id},{"result",result}};
    }catch(const std::exception& e){return error(id,-32602,e.what());}
}
Json Protocol::invoke(const std::string& name,const Json& a){
    auto call=[&](const std::string& method,const Json& p=Json{}){auto result=control_(method,p.is_null()?Json::object():p);if(!result.value("ok",false))throw std::runtime_error(result.value("error",std::string("Application rejected command")));return result;};
    auto command=[&](const std::string& action,Json extra=Json{}){extra["action"]=action;return content(call("command",extra));};
    if(name=="app_launch")return content(launch_());
    if(name=="app_status"){try{auto state=call("state");return content({{"running",true},{"connected",state["state"]["connected"]},{"model",state["state"]["model"]}});}catch(const std::exception& e){return content({{"running",false},{"detail",e.what()}});}}
    if(name=="get_control_schema")return content({{"settings",settingsSchema()},{"ui",uiSchema()},{"restart_required_settings",{"tick_ms","live_view_ms","download_timeout_seconds","busy_retries","retry_delay_ms","keep_alive_seconds","reconnect_attempts","reconnect_delay_ms","max_log_entries","window_width","window_height","property_labels"}},{"notes","Settings patches change the UI in memory; save_settings persists. Restart to apply worker/connection settings. Camera properties require camera_set_property. Read command_status; completed acknowledges command handling, not completion of autofocus/exposure."}});
    if(name=="get_state")return content(call("state"));
    if(name=="get_settings")return content(call("config"));
    if(name=="update_settings")return content(call("config_patch",a.at("patch")));
    if(name=="save_settings")return command("save_config");
    if(name=="update_ui")return content(call("ui_patch",a.at("patch")));
    if(name=="camera_properties"){auto s=call("state")["state"];return content({{"connected",s["connected"]},{"properties",s["properties"]},{"lens",s["lens"]}});}
    if(name=="camera_set_property"){
        if(a.contains("value")==a.contains("label"))throw std::invalid_argument("Supply exactly one of value or label");Json p={{"property",a.at("property")}};
        if(a.contains("value"))p["value"]=a["value"];
        else{int matches=0;const auto state=call("state");for(const auto& property:state["state"]["properties"])if(property["id"]==a["property"])for(const auto& choice:property["choices"])if(choice["label"]==a["label"]){p["value"]=choice["value"];++matches;}if(matches!=1)throw std::invalid_argument("Label not found or ambiguous; inspect camera_properties");}
        return command("set_property",p);
    }
    if(name=="camera_connection")return command(a.at("action"));
    if(name=="live_view")return command("live_view",{{"value",a.at("enabled")}});
    if(name=="lens_move")return command("lens_move",{{"value",a.at("step")}});
    if(name=="live_zoom")return command("zoom",{{"value",a.at("ratio")}});
    if(name=="capture_single")return command("capture_single");
    if(name=="sequence_control"){auto action=a.at("action").get<std::string>();return command(action=="stop"?"stop":"sequence_"+action);}
    if(name=="autofocus_control")return command("autofocus_"+a.at("action").get<std::string>());
    if(name=="command_status")return content(call("command_status",a));
    if(name=="get_logs")return content(call("logs",a));
    if(name=="get_analysis"){auto p=a;if(!p.contains("source"))p["source"]="live";return content(call("analysis",p));}
    if(name=="get_layout")return content(call("layout"));
    if(name=="set_layout")return content(call("layout",a));
    if(name=="app_window")return content(call("window",a));
    if(name=="get_image"){
        auto result=call("image",a);const auto data=result.at("data");result.erase("data");return {{"content",Json::array({{{"type","text"},{"text",result.dump()}},{{"type","image"},{"data",data},{"mimeType",result.at("mime_type")}}})},{"isError",false}};
    }
    throw std::invalid_argument("Unknown tool");
}
}
