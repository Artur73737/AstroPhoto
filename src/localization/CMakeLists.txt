add_library(astro_localization "${PROJECT_SOURCE_DIR}/src/localization/messages/src/Localization.cpp")
target_include_directories(astro_localization PUBLIC "${PROJECT_SOURCE_DIR}/src/localization/messages/include")
target_include_directories(astro_localization PRIVATE "${PROJECT_SOURCE_DIR}/src/localization/catalog/include")
