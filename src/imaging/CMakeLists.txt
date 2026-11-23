add_library(astro_preview "${PROJECT_SOURCE_DIR}/src/imaging/preview/photos/src/PhotoPreview.cpp" "${PROJECT_SOURCE_DIR}/src/imaging/peaking/edges/src/FocusPeaking.cpp" "${PROJECT_SOURCE_DIR}/src/imaging/peaking/stars/src/StarPeaking.cpp" "${PROJECT_SOURCE_DIR}/src/imaging/analysis/stars/src/StarAnalysis.cpp" "${PROJECT_SOURCE_DIR}/src/imaging/preview/live/src/LiveViewPreview.cpp" "${PROJECT_SOURCE_DIR}/src/imaging/analysis/histogram/src/ImageAnalysis.cpp")
target_include_directories(astro_preview PUBLIC
 "${PROJECT_SOURCE_DIR}/src/imaging/preview/processing/include"
 "${PROJECT_SOURCE_DIR}/src/imaging/analysis/histogram/include" "${PROJECT_SOURCE_DIR}/src/imaging/analysis/stars/include"
 "${PROJECT_SOURCE_DIR}/src/imaging/peaking/edges/include" "${PROJECT_SOURCE_DIR}/src/imaging/preview/live/include" "${PROJECT_SOURCE_DIR}/src/imaging/preview/photos/include")
target_link_libraries(astro_preview PUBLIC astro_core PRIVATE astro_libraw astro_fits astro_sensor astro_stacking windowscodecs ole32)
target_sources(astro_preview PRIVATE preview/processing/src/LiveProcessor.cpp)
target_include_directories(astro_preview PRIVATE "${PROJECT_SOURCE_DIR}/external/json/include")
