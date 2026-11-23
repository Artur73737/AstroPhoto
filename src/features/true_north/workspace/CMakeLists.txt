add_library(astro_zwo_ui "${PROJECT_SOURCE_DIR}/src/features/true_north/workspace/src/ZwoWorkspace.cpp")
target_include_directories(astro_zwo_ui PUBLIC "${PROJECT_SOURCE_DIR}/src/features/true_north/workspace/include" PRIVATE "${PROJECT_SOURCE_DIR}/external/json/include")
target_link_libraries(astro_zwo_ui PUBLIC astro_true_north astro_zwo astro_stacking imgui PRIVATE astro_polar_solve opengl32 ole32 astro_preview astro_fits)
