#pragma once
#include <filesystem>
#include <string>
#include <vector>
namespace astro::app::command_line {
struct RunOptions {
    bool smoke{}, light{}, night{}, connect{}, startLive{}, smokeStack{};
    int remotePort{},northDemo{};
    std::filesystem::path previewFile, pairingTestFile;
    std::string language;
    std::string mcpTransport;
    float dpi{};
    int width{}, height{};
};
void parseArguments(const std::vector<std::string>&,RunOptions&);
}
