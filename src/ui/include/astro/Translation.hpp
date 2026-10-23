#pragma once
#include <string_view>
#include <string>
namespace astro::ui {
void setLanguage(std::string_view language);
// Returns the input or a stable translation. Hidden ImGui IDs stay unchanged.
const char* tr(const char* text);
std::string translatedMessage(const std::string& text);
}
