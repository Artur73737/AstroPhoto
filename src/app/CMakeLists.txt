enable_language(RC)
configure_file("${PROJECT_SOURCE_DIR}/src/app/bootstrap/resources/app.rc.in" "${CMAKE_CURRENT_BINARY_DIR}/astrophoto.rc" @ONLY)
add_executable(AstroCanonController WIN32 "${PROJECT_SOURCE_DIR}/src/app/bootstrap/src/main.cpp" "${PROJECT_SOURCE_DIR}/src/platform/windows/night_display/src/NightDisplay.cpp"   "${PROJECT_SOURCE_DIR}/src/app/bootstrap/resources/app.manifest" "${CMAKE_CURRENT_BINARY_DIR}/astrophoto.rc")
set_target_properties(AstroCanonController PROPERTIES OUTPUT_NAME AstroController)
target_link_libraries(AstroCanonController PRIVATE astro_remote astro_stacking astro_update)
target_include_directories(AstroCanonController SYSTEM PRIVATE "${PROJECT_SOURCE_DIR}/external/httplib")
target_link_libraries(AstroCanonController PRIVATE astro_mcp_protocol astro_ui astro_infrastructure astro_preview imgui windowscodecs ole32 dwmapi advapi32 crypt32 bcrypt shell32 ws2_32)
add_custom_command(TARGET AstroCanonController POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/src/configuration/defaults/resources/defaults.json" "$<TARGET_FILE_DIR:AstroCanonController>/defaults.json")
add_custom_command(TARGET AstroCanonController POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/external/imgui/misc/fonts/Cousine-Regular.ttf" "$<TARGET_FILE_DIR:AstroCanonController>/Cousine-Regular.ttf")
add_custom_command(TARGET AstroCanonController POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/licenses/nina-livestack" "$<TARGET_FILE_DIR:AstroCanonController>/licenses/nina-livestack"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/src/stacking/registration/geometry/src/NinaGeometry.cpp" "${CMAKE_SOURCE_DIR}/src/stacking/model/include/astro/stacking/model/StackMath.hpp" "$<TARGET_FILE_DIR:AstroCanonController>/licenses/nina-livestack"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/licenses/hocus-focus" "$<TARGET_FILE_DIR:AstroCanonController>/licenses/hocus-focus"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/src/core/focus/metrics/src/HocusFocusMath.cpp" "${CMAKE_SOURCE_DIR}/src/core/focus/metrics/include/astro/core/focus/metrics/HocusFocusMath.hpp" "$<TARGET_FILE_DIR:AstroCanonController>/licenses/hocus-focus"
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:AstroCanonController>/licenses/LibRaw"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/external/libraw/LICENSE.CDDL" "${CMAKE_SOURCE_DIR}/external/libraw/COPYRIGHT" "$<TARGET_FILE_DIR:AstroCanonController>/licenses/LibRaw")
add_custom_command(TARGET AstroCanonController POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/licenses/erfa" "$<TARGET_FILE_DIR:AstroCanonController>/licenses/erfa"
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/docs/true-north" "$<TARGET_FILE_DIR:AstroCanonController>/docs/true-north")
add_custom_command(TARGET AstroCanonController POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/licenses/zwo" "$<TARGET_FILE_DIR:AstroCanonController>/licenses/zwo")
target_link_libraries(AstroCanonController PRIVATE astro_control_api astro_mcp_transport astro_cli_options)
target_include_directories(AstroCanonController PRIVATE "${PROJECT_SOURCE_DIR}/src/platform/windows/night_display/include" "${PROJECT_SOURCE_DIR}/src/platform/windows/taskbar/include")
if(ASTRO_WITH_ZWO)
 target_compile_definitions(AstroCanonController PRIVATE ASTRO_WITH_ZWO)
 add_custom_command(TARGET AstroCanonController POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${ASI_ROOT}/lib/x64/ASICamera2.dll" "$<TARGET_FILE_DIR:AstroCanonController>")
endif()
if(ASTRO_WITH_EDSDK)
 astro_copy_edsdk_runtime(AstroCanonController)
endif()

