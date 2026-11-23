#pragma once
#include <astro/core/domain/Core.hpp>
namespace astro::zwo { std::unique_ptr<ICamera> createAdapter();
bool available(); }
