#pragma once
#include <astro/Core.hpp>
namespace astro {
class ConfigManager {
public:
    static Config load(const std::filesystem::path&);
    static void save(const std::filesystem::path&, const Config&);
};
}
