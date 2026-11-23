target_sources(astro_camera_models PRIVATE
  module/src/Module.cpp capture/src/Capture.cpp focus/src/Focus.cpp live_view/src/LiveView.cpp)
astro_register_camera_module(canonEos1100d)
if(BUILD_TESTING)
  set_property(GLOBAL APPEND PROPERTY ASTRO_CAMERA_MODULE_TESTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/unit/ModuleTests.cpp")
endif()

target_include_directories(astro_camera_models PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/module/include")
