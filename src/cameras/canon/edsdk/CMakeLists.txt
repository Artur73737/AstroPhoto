add_library(astro_edsdk "${PROJECT_SOURCE_DIR}/src/cameras/canon/edsdk/camera/src/EdsCamera.cpp" "${PROJECT_SOURCE_DIR}/src/cameras/canon/edsdk/properties/src/PropertyLabels.cpp")
target_include_directories(astro_edsdk PUBLIC "${PROJECT_SOURCE_DIR}/src/cameras/canon/edsdk/camera/include" "${PROJECT_SOURCE_DIR}/src/cameras/canon/edsdk/properties/include")
target_link_libraries(astro_edsdk PUBLIC astro_core astro_camera_models PRIVATE EDSDK::EDSDK ole32)
