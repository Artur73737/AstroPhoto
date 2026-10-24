#pragma once
#include <nlohmann/json.hpp>
#include <functional>
#include <optional>
namespace astro::mcp {
using Json=nlohmann::json;
using Control=std::function<Json(const std::string&,const Json&)>;
// Transport-independent MCP JSON-RPC implementation; no UI or SDK dependency.
class Protocol {
public:
    Protocol(Control control,std::function<Json()> launch);
    std::optional<Json> dispatch(const Json&);
    Json tools() const;
private:
    Control control_;std::function<Json()> launch_;
    Json invoke(const std::string&,const Json&);
};
Json error(const Json& id,int code,const std::string& message);
}
