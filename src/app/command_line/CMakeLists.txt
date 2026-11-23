add_library(astro_cli_options "${PROJECT_SOURCE_DIR}/src/app/command_line/src/Options.cpp")
target_include_directories(astro_cli_options PUBLIC "${PROJECT_SOURCE_DIR}/src/app/command_line/include")
target_link_libraries(astro_cli_options PUBLIC astro_core)
