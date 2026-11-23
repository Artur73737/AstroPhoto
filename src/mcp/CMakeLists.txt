add_library(astro_mcp_protocol "${PROJECT_SOURCE_DIR}/src/mcp/protocol/src/McpProtocol.cpp")
target_include_directories(astro_mcp_protocol PUBLIC "${PROJECT_SOURCE_DIR}/src/mcp/protocol/include" "${PROJECT_SOURCE_DIR}/external/json/include" PRIVATE "${PROJECT_BINARY_DIR}/generated")
target_link_libraries(astro_mcp_protocol PUBLIC astro_configuration astro_control_queue)
add_library(astro_mcp_transport "${PROJECT_SOURCE_DIR}/src/mcp/transport/src/McpTransport.cpp")
target_include_directories(astro_mcp_transport PUBLIC "${PROJECT_SOURCE_DIR}/src/mcp/transport/include" PRIVATE "${PROJECT_SOURCE_DIR}/external/httplib")
target_link_libraries(astro_mcp_transport PUBLIC astro_mcp_protocol PRIVATE shell32 ole32 bcrypt ws2_32)

