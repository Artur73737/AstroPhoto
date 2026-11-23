add_executable(astro_install_zwo_driver "${PROJECT_SOURCE_DIR}/src/installer/drivers/zwo/src/InstallZwoDriver.cpp")
set_property(TARGET astro_install_zwo_driver PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
