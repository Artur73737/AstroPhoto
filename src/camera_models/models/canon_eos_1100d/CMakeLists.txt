target_sources(astro_camera_models PRIVATE
  src/Module.cpp src/capture/Capture.cpp src/focus/Focus.cpp src/live_view/LiveView.cpp)
astro_register_camera_module(canonEos1100d)
if(BUILD_TESTING)
  set_property(GLOBAL APPEND PROPERTY ASTRO_CAMERA_MODULE_TESTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/ModuleTests.cpp")
endif()
