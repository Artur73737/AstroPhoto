#include <astro/app/command_line/Options.hpp>
#include <astro/core/domain/Core.hpp>
#include <cmath>
namespace astro::app::command_line {
namespace {
int integer(const std::string& value) {
    std::size_t used{};
    int result = std::stoi(value, &used);
    if (used != value.size()) throw std::invalid_argument("Invalid integer argument");
    return result;
}
float real(const std::string& value) {
    std::size_t used{};
    float result = std::stof(value, &used);
    if (used != value.size() || !std::isfinite(result)) throw std::invalid_argument("Invalid numeric argument");
    return result;
}
}  // namespace
void parseArguments(const std::vector<std::string>& arguments, RunOptions& options) {
    for (const auto& arg : arguments) {
        if (arg == "--smoke-test")
            options.smoke = true;
        else if (arg.starts_with("--true-north-demo=")) {
            options.northDemo = integer(arg.substr(18));
            if (options.northDemo < 1 || options.northDemo > 3)
                throw std::invalid_argument("True North image index must be 1, 2 or 3");
        } else if (arg == "--mcp-stdio" || arg == "--mcp-http") {
            if (!options.mcpTransport.empty()) throw std::invalid_argument("Choose only one MCP transport");
            options.mcpTransport = arg == "--mcp-stdio" ? "stdio" : "http";
        } else if (arg == "--connect")
            options.connect = true;
        else if (arg == "--live-view") {
            options.connect = true;
            options.startLive = true;
        } else if (arg.starts_with("--remote-server=")) {
            options.remotePort = integer(arg.substr(16));
            if (options.remotePort < 1024 || options.remotePort > 65535)
                throw std::invalid_argument("Remote port must be 1024..65535");
        } else if (arg.starts_with("--pairing-test-file="))
            options.pairingTestFile = astro::pathFromUtf8(arg.substr(20));
        else if (arg == "--smoke-stack")
            options.smokeStack = true;
        else if (arg == "--smoke-light")
            options.light = true;
        else if (arg == "--smoke-night")
            options.night = true;
        else if (arg.starts_with("--smoke-language="))
            options.language = arg.substr(17);
        else if (arg.starts_with("--smoke-preview="))
            options.previewFile = astro::pathFromUtf8(arg.substr(16));
        else if (arg.starts_with("--smoke-dpi="))
            options.dpi = real(arg.substr(12));
        else if (arg.starts_with("--smoke-width="))
            options.width = integer(arg.substr(14));
        else if (arg.starts_with("--smoke-height="))
            options.height = integer(arg.substr(15));
        else
            throw std::invalid_argument("Argomento non riconosciuto: " + arg);
    }
    if (!std::isfinite(options.dpi) || options.dpi < 0 || options.dpi > 4 || options.width < 0 || options.height < 0 ||
        options.width > 7680 || options.height > 4320)
        throw std::invalid_argument("Parametri smoke non validi");
    if (!options.smoke && (options.smokeStack || options.dpi || options.width || options.height || options.light ||
                           options.night || !options.previewFile.empty() || !options.language.empty()))
        throw std::invalid_argument("Le opzioni di test richiedono --smoke-test");
    if (!options.language.empty() && options.language != "it" && options.language != "en")
        throw std::invalid_argument("Unsupported smoke language");
    if (options.smoke && options.connect)
        throw std::invalid_argument("Lo smoke test non apre sessioni sulla fotocamera");
}
}  // namespace astro::app::command_line
