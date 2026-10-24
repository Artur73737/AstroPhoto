#pragma once
#include <astro/Core.hpp>
namespace astro {
class ConfigManager {
public:
    static Config load(const std::filesystem::path&);
    static Config fromJson(const std::string&);
    static std::string toJson(const Config&);
    static void save(const std::filesystem::path&, const Config&);
};
}
