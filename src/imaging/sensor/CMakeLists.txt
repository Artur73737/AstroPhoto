add_library(astro_sensor src/SensorImage.cpp src/SensorFiles.cpp)
target_include_directories(astro_sensor PUBLIC include)
target_include_directories(astro_sensor PRIVATE "${PROJECT_SOURCE_DIR}/external/json/include")
target_link_libraries(astro_sensor PRIVATE windowscodecs ole32)
