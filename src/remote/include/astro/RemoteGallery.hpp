#pragma once
#include <nlohmann/json.hpp>
#include <filesystem>
#include <map>
namespace astro::remote {
class Gallery {
public:
    nlohmann::json list(const std::filesystem::path& root);
    std::string image(const std::filesystem::path& root,const std::string& id);
private:std::map<std::string,std::filesystem::path> files_;std::filesystem::path root_;
};
}
