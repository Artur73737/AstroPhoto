#pragma once
#include <string>
#include <string_view>
namespace astro {
// Pure, thread-safe bilingual catalog. Unknown external diagnostics remain intact.
std::string localizeMessage(std::string_view text,std::string_view language);
std::string localizeLogLevel(std::string_view level,std::string_view language);
}
