add_library(astro_configuration "${PROJECT_SOURCE_DIR}/src/configuration/persistence/src/ConfigManager.cpp")
target_include_directories(astro_configuration PUBLIC "${PROJECT_SOURCE_DIR}/src/configuration/persistence/include" PRIVATE "${PROJECT_SOURCE_DIR}/external/json/include")
target_link_libraries(astro_configuration PUBLIC astro_core astro_camera_models)
