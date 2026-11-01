#define NOMINMAX
#include <windows.h>
#include <astro/RemoteAuth.hpp>
#include <astro/RemoteMedia.hpp>
#include <astro/RemoteGallery.hpp>
#include <astro/RemoteServer.hpp>
#include "ControlQueue.hpp"
#include <ixwebsocket/IXWebSocket.h>
#include <ixwebsocket/IXGetFreePort.h>
#include <gtest/gtest.h>
#include <fstream>
#include <condition_variable>
#include <thread>
using namespace astro::remote;
using namespace std::string_literals;
using Json=nlohmann::json;
namespace {
Json proof(Session& session,const std::string& key,const std::string& name="Integration client"){auto challenge=session.challenge("Computer");return {{"type","authenticate"},{"device_name",name},{"proof",hmac(key,"auth\n"+challenge.at("nonce").get<std::string>()+"\n"+name)}};}
Json packet(Session& session,const std::string& key,std::uint64_t sequence,const Json& body){auto payload=body.dump();return {{"seq",sequence},{"payload",payload},{"mac",hmac(key,"client\n"+session.challenge("Computer").at("nonce").get<std::string>()+"\n"+std::to_string(sequence)+"\n"+payload)}};}
template<class F> bool eventually(F predicate){for(int i=0;i<200;++i){if(predicate())return true;std::this_thread::sleep_for(std::chrono::milliseconds(20));}return false;}
}
TEST(RemoteAuth,Rfc4231HmacVector){EXPECT_EQ(hmac(std::string(20,'\x0b'),"Hi There"),"b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7");}
TEST(RemoteAuth,WrongKeyAndEmptyDeviceRejected){Session s(randomSecret());
EXPECT_THROW(s.authenticate(proof(s,randomSecret())),std::runtime_error);
EXPECT_FALSE(s.authenticated());
EXPECT_THROW(s.authenticate(proof(s,"wrong","")),std::runtime_error);}
TEST(RemoteAuth,RejectsReplayTamperingReflectionAndOtherConnection){
    const auto key=randomSecret();Session s(key),other(key);s.authenticate(proof(s,key));other.authenticate(proof(other,key));
    const auto wire=packet(s,key,1,{{"tool","get_state"}});
EXPECT_EQ(s.receive(wire).at("tool"),"get_state");
EXPECT_THROW(s.receive(wire),std::runtime_error);
EXPECT_THROW(other.receive(wire),std::runtime_error);
    auto altered=packet(s,key,2,{{"tool","capture_single"}});altered["payload"]="{}";
EXPECT_THROW(s.receive(altered),std::runtime_error);
    EXPECT_THROW(s.receive(s.send({{"tool","get_state"}})),std::runtime_error);
}
TEST(RemoteAuth,BinaryFrameSignedWithSameMonotonicSequence){
    const auto key=randomSecret();Session s(key);s.authenticate(proof(s,key));s.send({{"type","authenticated"}});
    const std::string jpeg="\xff\xd8payload\0withnull"s;
    const auto binary=s.sendImage({{"id",19},{"mime_type","image/jpeg"}},jpeg);unsigned length{};for(int i=0;i<4;++i)length=(length<<8)|static_cast<unsigned char>(binary[i]);
    auto header=Json::parse(binary.substr(4,length));
EXPECT_EQ(header.at("seq"),2);
EXPECT_EQ(binary.substr(4+length),jpeg);
    const auto body=header.at("payload").get<std::string>();
EXPECT_EQ(header.at("mac"),hmac(key,"server-image\n"+s.challenge("Computer").at("nonce").get<std::string>()+"\n2\n"+body+"\n"+jpeg));
    EXPECT_EQ(s.send({{"done",true}}).at("seq"),3);
}
TEST(RemoteMedia,JpegIsBoundedAndDecodableOffUiThread){
    astro::PreviewPixels pixels;pixels.width=2200;pixels.height=1200;pixels.rgba.resize(std::size_t(pixels.width)*pixels.height*4,128);
    const auto jpeg=previewJpeg(std::move(pixels));
ASSERT_LT(jpeg.size(),4*1024*1024u);
ASSERT_EQ(static_cast<unsigned char>(jpeg[0]),0xff);
    auto encoded=cameraJpeg(jpeg);
ASSERT_FALSE(encoded.empty());CoInitializeEx(nullptr,COINIT_MULTITHREADED);auto decoded=astro::decodeImage({encoded.begin(),encoded.end()});CoUninitialize();
EXPECT_EQ(decoded.width,1024);
EXPECT_LE(decoded.height,1024);
}
TEST(RemoteGallery,OnlyListedFilesWithinCurrentOutputFolderCanBeOpened){
    auto directory=std::filesystem::temp_directory_path()/randomSecret();std::filesystem::create_directories(directory);struct Cleanup{std::filesystem::path p;~Cleanup(){std::filesystem::remove_all(p);}} cleanup{directory};
    std::ofstream(directory/"metadata.json")<<"{}";Gallery gallery;
EXPECT_TRUE(gallery.list(directory).at("items").empty());
EXPECT_THROW(gallery.image(directory,"../outside.jpg"),std::runtime_error);
    astro::PreviewPixels pixels;pixels.width=8;pixels.height=8;pixels.rgba.resize(8*8*4,100);auto jpeg=previewJpeg(std::move(pixels));std::ofstream file(directory/"capture.jpg",std::ios::binary);file.write(jpeg.data(),jpeg.size());file.close();
    auto listing=gallery.list(directory);
ASSERT_EQ(listing.at("items").size(),1);auto id=listing["items"][0]["id"].get<std::string>();
EXPECT_FALSE(gallery.image(directory,id).empty());
EXPECT_THROW(gallery.image(directory/"different",id),std::runtime_error);
}
TEST(RemoteMedia,PreservesCameraModulePreviewCropForTenTimesZoom){
    astro::PreviewPixels pixels;pixels.width=800;pixels.height=600;pixels.rgba.resize(800*600*4,100);auto jpeg=previewJpeg(std::move(pixels));
    astro::LiveFrame frame;frame.previewRegion.x=.25;frame.previewRegion.y=.25;frame.previewRegion.width=.5;frame.previewRegion.height=.5;
    const auto cropped=cameraJpeg(jpeg,frame);CoInitializeEx(nullptr,COINIT_MULTITHREADED);auto decoded=astro::decodeImage({cropped.begin(),cropped.end()});CoUninitialize();
    EXPECT_EQ(decoded.width,400);EXPECT_EQ(decoded.height,300);
}
TEST(RemoteServer,RealSocketPairingCatalogReplayAndRestart){
    astro::ControlQueue queue;auto settings=std::filesystem::temp_directory_path()/(randomSecret()+".json");Server server(queue,settings);
    const int port=ix::getFreePort();server.start(port);
ASSERT_TRUE(eventually([&]{return server.status().running;}))<<server.status().error;const auto key=server.status().token;
    ix::WebSocket socket;socket.disableAutomaticReconnection();socket.setExtraHeaders({{"Origin",""}});socket.setUrl("ws://127.0.0.1:"+std::to_string(port)+"/astrophoto");
    std::mutex mutex;std::condition_variable changed;std::vector<Json> messages;std::atomic<bool> closed{};
    socket.setOnMessageCallback([&](const ix::WebSocketMessagePtr& msg){if(msg->type==ix::WebSocketMessageType::Message){std::lock_guard lock(mutex);messages.push_back(Json::parse(msg->str));changed.notify_all();}if(msg->type==ix::WebSocketMessageType::Close)closed=true;});socket.start();
    auto next=[&]()->Json{std::unique_lock lock(mutex);if(!changed.wait_for(lock,std::chrono::seconds(5),[&]{return !messages.empty();}))return Json{};auto j=messages.front();messages.erase(messages.begin());return j;};
    auto challenge=next();
ASSERT_EQ(challenge.value("type",std::string{}),"challenge");const auto nonce=challenge.at("nonce").get<std::string>();
    socket.sendText(Json{{"type","authenticate"},{"device_name","Socket integration test"},{"proof",hmac(key,"auth\n"+nonce+"\nSocket integration test")}}.dump());
    auto authenticated=next();
ASSERT_TRUE(authenticated.contains("payload"));
EXPECT_EQ(Json::parse(authenticated["payload"].get<std::string>()).at("type"),"authenticated");
    const auto payload=Json{{"id",1},{"tool","tools"}}.dump();const auto request=Json{{"seq",1},{"payload",payload},{"mac",hmac(key,"client\n"+nonce+"\n1\n"+payload)}}.dump();socket.sendText(request);
    auto response=next();
ASSERT_TRUE(response.contains("payload"));
EXPECT_GE(Json::parse(response["payload"].get<std::string>()).at("result").size(),20);
EXPECT_TRUE(server.status().token.empty());
ASSERT_EQ(server.status().peers.size(),1);
EXPECT_EQ(server.status().peers[0].name,"Socket integration test");
    socket.sendText(request);
EXPECT_TRUE(eventually([&]{return closed.load();}));socket.stop();server.stop();
ASSERT_TRUE(eventually([&]{return !server.status().running&&!server.status().busy;}));server.start(port);
EXPECT_TRUE(eventually([&]{return server.status().running;}));
EXPECT_NE(server.status().token,key);server.stop();queue.close();std::error_code error;std::filesystem::remove(settings,error);
}
