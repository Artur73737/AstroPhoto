#define NOMINMAX
#include <httplib.h>
#include "McpTransport.hpp"
#include <gtest/gtest.h>
#include <cstdlib>
TEST(McpTransport,ReservedConfiguredPortFallsBackWithoutDisablingAuthentication){
    if(!std::getenv("ASTRO_MCP_TRANSPORT_TEST"))GTEST_SKIP()<<"Opt in: binds loopback and persists a replacement port if necessary";
    astro::ControlQueue queue;astro::mcp::Server server(queue,"http");
    const auto url=server.endpoint();ASSERT_TRUE(url.starts_with("http://127.0.0.1:"));
    const auto port=std::stoi(url.substr(std::string("http://127.0.0.1:").size()));ASSERT_GE(port,1024);
    httplib::Client client("127.0.0.1",port);client.set_connection_timeout(2);
    auto response=client.Post("/mcp","{}","application/json");ASSERT_TRUE(response);EXPECT_EQ(response->status,401);
    EXPECT_FALSE(server.finished());
}
