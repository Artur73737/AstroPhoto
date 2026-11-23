add_library(astro_update "${PROJECT_SOURCE_DIR}/src/services/updates/src/UpdateCheck.cpp")
target_include_directories(astro_update PUBLIC "${PROJECT_SOURCE_DIR}/src/services/updates/include" PRIVATE "${PROJECT_SOURCE_DIR}/external/json/include")
target_compile_definitions(astro_update PRIVATE ASTRO_UPDATE_REPOSITORY="${ASTRO_UPDATE_REPOSITORY}")
target_link_libraries(astro_update PRIVATE winhttp)
