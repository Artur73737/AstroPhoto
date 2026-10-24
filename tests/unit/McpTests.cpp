#include "McpProtocol.hpp"
#include "ControlQueue.hpp"
#include <astro/ConfigManager.hpp>
#include <gtest/gtest.h>
#include <atomic>
#include <thread>
using astro::mcp::Json;
namespace {
Json tool(const std::string& name,Json args=Json::object()){return {{"jsonrpc","2.0"},{"id",7},{"method","tools/call"},{"params",{{"name",name},{"arguments",args}}}};}
struct ProtocolTest:testing::Test {
    int calls{};Json last;std::string method;
    astro::mcp::Protocol protocol{
        [&](const std::string& m,const Json& p){++calls;method=m;last=p;return Json{{"ok",true},{"id",123},{"status","queued"}};},
        []{return Json{{"running",true}};}};
};
TEST_F(ProtocolTest,NegotiatesVersionAndListsCompleteSchemas){
    const auto r=protocol.dispatch({{"jsonrpc","2.0"},{"id","init"},{"method","initialize"},{"params",{{"protocolVersion","2025-11-25"}}}});
    ASSERT_TRUE(r);EXPECT_EQ((*r)["result"]["protocolVersion"],"2025-11-25");EXPECT_EQ((*r)["id"],"init");
    const auto tools=protocol.tools();EXPECT_EQ(tools.size(),24);EXPECT_EQ(calls,0);
    for(const auto& t:tools){EXPECT_TRUE(t.contains("inputSchema"));EXPECT_TRUE(t["annotations"].contains("readOnlyHint"));}
    auto schema=protocol.dispatch(tool("get_control_schema"))->at("result").at("structuredContent");
    const auto config=Json::parse(astro::ConfigManager::toJson(astro::Config{}));
    for(auto it=config.begin();it!=config.end();++it)if(it.key()!="schema_version")EXPECT_TRUE(schema["settings"]["properties"].contains(it.key()))<<it.key();
    for(auto it=config["star_autofocus"].begin();it!=config["star_autofocus"].end();++it)EXPECT_TRUE(schema["settings"]["properties"]["star_autofocus"]["properties"].contains(it.key()))<<it.key();
}
TEST_F(ProtocolTest,InvalidAndUnknownMessagesNeverMutate){
    EXPECT_EQ(protocol.dispatch(Json::array())->at("error")["code"],-32600);
    EXPECT_EQ(protocol.dispatch({{"jsonrpc","2.0"},{"id",1},{"method","unknown"}})->at("error")["code"],-32601);
    EXPECT_EQ(protocol.dispatch(tool("unknown"))->at("error")["code"],-32602);
    EXPECT_FALSE(protocol.dispatch({{"jsonrpc","2.0"},{"method","notifications/initialized"}}));EXPECT_EQ(calls,0);
}
TEST_F(ProtocolTest,WrongTypesRangesAndUnknownFieldsAreRejected){
    for(const auto& request:{tool("lens_move",{{"step",0}}),tool("lens_move",{{"step",1.5}}),tool("live_view",{{"enabled",1}}),tool("live_zoom",{{"ratio",2}}),tool("update_settings",{{"patch",{{"language","xx"}}}}),tool("update_ui",{{"patch",{{"focus_target",{{"u",2.0}}}}}}),tool("get_state",{{"oops",true}})}){
        auto reply=protocol.dispatch(request);ASSERT_TRUE(reply);EXPECT_TRUE((*reply)["result"]["isError"].get<bool>());
    }EXPECT_EQ(calls,0);
}
TEST_F(ProtocolTest,CommandsReturnAcknowledgementIdsNotFakeCompletion){
    auto reply=protocol.dispatch(tool("lens_move",{{"step",-3}}));ASSERT_TRUE(reply);
    EXPECT_EQ(method,"command");EXPECT_EQ(last["action"],"lens_move");EXPECT_EQ(last["value"],-3);EXPECT_EQ((*reply)["result"]["structuredContent"]["status"],"queued");
}
TEST_F(ProtocolTest,NestedPatchPreservesOmittedValues){
    protocol.dispatch(tool("update_settings",{{"patch",{{"star_autofocus",{{"min_stars",7}}},{"sequence",Json::array()}}}}));
    EXPECT_EQ(method,"config_patch");EXPECT_EQ(last["star_autofocus"].size(),1);EXPECT_TRUE(last["sequence"].empty());
}
TEST(McpProtocol,NativeErrorsBecomeToolErrors){
    astro::mcp::Protocol p([](const std::string&,const Json&){return Json{{"ok",false},{"error","Lens is MF"}};},[]{return Json{};});
    const auto r=p.dispatch(tool("lens_move",{{"step",1}}));EXPECT_TRUE((*r)["result"]["isError"].get<bool>());EXPECT_EQ((*r)["result"]["content"][0]["text"],"Lens is MF");
}
TEST(McpProtocol,ImageUsesNativeImageContentWithoutDuplicatingBase64InText){
    astro::mcp::Protocol p([](const std::string&,const Json&){return Json{{"ok",true},{"mime_type","image/png"},{"data","aGVsbG8="},{"width",1}};},[]{return Json{};});
    const auto r=p.dispatch(tool("get_image"));const auto& blocks=(*r)["result"]["content"];ASSERT_EQ(blocks.size(),2);EXPECT_EQ(blocks[1]["type"],"image");EXPECT_EQ(blocks[1]["mimeType"],"image/png");EXPECT_EQ(blocks[1]["data"],"aGVsbG8=");EXPECT_EQ(blocks[0]["text"].get<std::string>().find("aGVsbG8="),std::string::npos);
}
TEST(ControlQueue,ConcurrentProducersHaveIsolatedRepliesOnTheUiThread){
    astro::ControlQueue queue;std::atomic<int> completed{},errors{};std::vector<std::thread> producers;const auto ui=std::this_thread::get_id();
    for(int n=0;n<8;++n)producers.emplace_back([&,n]{try{for(int i=0;i<30;++i){const auto text=std::to_string(n)+":"+std::to_string(i);if(queue.request(text)!=text+"!")++errors;}}catch(...){++errors;}++completed;});
    int handled{};while(completed<8){queue.pump([&](const std::string& s){EXPECT_EQ(std::this_thread::get_id(),ui);++handled;return s+"!";});std::this_thread::sleep_for(std::chrono::milliseconds(1));}
    for(auto& p:producers)p.join();EXPECT_EQ(errors,0);EXPECT_EQ(handled,240);
}
TEST(ControlQueue,ExpiredRequestIsNeverExecutedLater){
    astro::ControlQueue queue;auto waiter=std::async(std::launch::async,[&]{return queue.request("capture",std::chrono::milliseconds(20));});
    EXPECT_THROW(waiter.get(),std::runtime_error);int executed{};queue.pump([&](const std::string&){++executed;return "bad";});EXPECT_EQ(executed,0);
}
TEST(ControlQueue,ShutdownWakesWaitersAndRejectsNewWork){
    astro::ControlQueue queue;auto waiter=std::async(std::launch::async,[&]{return queue.request("pending");});queue.close();
    ASSERT_EQ(waiter.wait_for(std::chrono::seconds(1)),std::future_status::ready);
    EXPECT_THROW(waiter.get(),std::runtime_error);
    EXPECT_THROW(queue.request("late"),std::runtime_error);
}
TEST(ControlQueue,HandlerExceptionsPropagateWithoutHanging){
    astro::ControlQueue queue;auto waiter=std::async(std::launch::async,[&]{return queue.request("invalid");});
    while(waiter.wait_for(std::chrono::milliseconds(1))!=std::future_status::ready)queue.pump([](const std::string&)->std::string{throw std::invalid_argument("invalid value");});
    EXPECT_THROW(waiter.get(),std::invalid_argument);
}
TEST(ControlQueue,CapacityIsBounded){astro::ControlQueue queue(0);EXPECT_THROW(queue.request("overflow"),std::runtime_error);}
TEST(McpConfiguration,InMemoryRoundTripAndInvalidDraftRemainReadable){
    astro::Config c;c.sequence.push_back({});c.starFocus.searchSpan=25;c.focusPeaking=true;
    const auto text=astro::ConfigManager::toJson(c);EXPECT_EQ(astro::ConfigManager::toJson(astro::ConfigManager::fromJson(text)),text);
    c.singleCapture.exposureSeconds=0;const auto draft=astro::ConfigManager::toJson(c);EXPECT_FALSE(draft.empty());EXPECT_THROW(astro::ConfigManager::fromJson(draft),std::invalid_argument);
}
TEST(McpMailbox,CommandIdsErrorsAndBackPressure){
    astro::Mailbox box;auto first=box.submit({astro::Action::Connect});auto second=box.submit({astro::Action::Disconnect});EXPECT_NE(first,second);EXPECT_FALSE(box.result(first)->finished);
    auto drained=box.take();ASSERT_EQ(drained.size(),2);EXPECT_EQ(drained[0].id,first);box.complete(first,"USB unavailable");box.complete(second);
    EXPECT_EQ(box.result(first)->error,"USB unavailable");EXPECT_TRUE(box.result(second)->finished);EXPECT_TRUE(box.result(second)->error.empty());EXPECT_FALSE(box.result(999));
    for(int i=0;i<64;++i)box.submit({});EXPECT_THROW(box.submit({}),std::runtime_error);
}
}
