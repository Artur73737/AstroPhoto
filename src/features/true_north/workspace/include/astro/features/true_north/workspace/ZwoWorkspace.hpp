#pragma once
#include <string>
#include <astro/core/domain/Core.hpp>
namespace astro::ui {
void bindNorthCamera(Mailbox&, const Config&);
void zwoNorthWorkspace();
std::string zwoNorthControl(const std::string& request);
}  // namespace astro::ui
