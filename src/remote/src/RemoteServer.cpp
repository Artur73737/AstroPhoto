#define NOMINMAX
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <astro/RemoteServer.hpp>
#include <astro/RemoteAuth.hpp>
#include <astro/RemoteGallery.hpp>
#include <astro/RemoteMedia.hpp>
#include <astro/Localization.hpp>
#include "ControlQueue.hpp"
#include "McpProtocol.hpp"
#include <ixwebsocket/IXWebSocketServer.h>
#include <ixwebsocket/IXNetSystem.h>
#include <mutex>
#include <thread>
#include <atomic>
#include <fstream>
#include <map>
#include <chrono>
namespace astro::remote {
using Json=nlohmann::json;
using Clock=std::chrono::steady_clock;
struct Server::Impl {
    struct Client {
        explicit Client(const std::string& key):auth(key){}
        Session auth;Gallery gallery;Clock::time_point opened{Clock::now()},window{opened};
        std::atomic<bool> authenticated{};int requests{};std::string id,address;
        Clock::time_point lastImage{};
    };
    ControlQueue& control;std::filesystem::path settings;mutable std::mutex mutex;
    RemoteStatus state;std::map<ix::WebSocket*,std::shared_ptr<Client>> clients;
    std::atomic<bool> quit{};std::atomic<int> desired{};std::atomic<unsigned> generation{};
    std::thread worker;
    Clock::time_point pairingDeadline{};bool pairingUsed{};
    Impl(ControlQueue& queue,std::filesystem::path file):control(queue),settings(std::move(file)){
        wchar_t name[256]{};DWORD size=static_cast<DWORD>(std::size(name));if(!GetComputerNameW(name,&size))throw std::runtime_error("Cannot read computer name");const auto length=WideCharToMultiByte(CP_UTF8,0,name,size,nullptr,0,nullptr,nullptr);state.host.resize(length);WideCharToMultiByte(CP_UTF8,0,name,size,state.host.data(),length,nullptr,nullptr);
        try{std::ifstream input(settings);Json j;input>>j;int p=j.at("port");if(p>=1024&&p<=65535)state.port=p;}catch(...){}
        worker=std::thread([this]{run();});
    }
    ~Impl(){quit=true;if(worker.joinable())worker.join();}
    void log(const std::string& level,const std::string& message){
        SYSTEMTIME now{};GetLocalTime(&now);char time[16]{};sprintf_s(time,"%02u:%02u:%02u",now.wHour,now.wMinute,now.wSecond);
        std::lock_guard lock(mutex);state.logs.push_back({time,level,message});if(state.logs.size()>500)state.logs.erase(state.logs.begin());
    }
    static void englishDiagnostics(Json& value){
        if(value.is_array()){for(auto& item:value)englishDiagnostics(item);return;}
        if(!value.is_object())return;
        for(auto it=value.begin();it!=value.end();++it){if(it->is_string()&&(it.key()=="detail"||it.key()=="error"||it.key()=="message"||it.key()=="backend_status"||it.key()=="state"||it.key()=="label")) *it=localizeMessage(it->get<std::string>(),"en");else if(it->is_structured())englishDiagnostics(*it);}
    }
    Json call(const std::string& method,const Json& params){auto result=Json::parse(control.request(Json{{"method",method},{"params",params}}.dump()));if(method=="state"||method=="logs"||method=="command_status")englishDiagnostics(result);return result;}
    std::filesystem::path output(){auto j=call("config",Json::object());auto path=pathFromUtf8(j.at("config").at("output_directory").get<std::string>());return path.is_absolute()?path:settings.parent_path()/path;}
    void message(const std::shared_ptr<ix::ConnectionState>& connection,ix::WebSocket& socket,const ix::WebSocketMessagePtr& msg,const std::string& key){
        if(msg->type==ix::WebSocketMessageType::Open){
            if(msg->openInfo.uri!="/astrophoto"||(msg->openInfo.headers.count("Origin")&&!msg->openInfo.headers.at("Origin").empty())){socket.close(1008,"Native clients only");return;}
            auto client=std::make_shared<Client>(key);client->id=connection->getId();client->address=connection->getRemoteIp();
            {std::lock_guard lock(mutex);clients[&socket]=client;}
            socket.sendText(client->auth.challenge(state.host).dump());return;
        }
        std::shared_ptr<Client> client;
        {std::lock_guard lock(mutex);auto it=clients.find(&socket);if(it!=clients.end())client=it->second;}
        if(!client)return;
        if(msg->type==ix::WebSocketMessageType::Close||msg->type==ix::WebSocketMessageType::Error){
            {std::lock_guard lock(mutex);clients.erase(&socket);std::erase_if(state.peers,[&](const auto& peer){return peer.id==client->id;});}
            if(client->authenticated)log("Info","Device disconnected: "+client->auth.device());return;
        }
        if(msg->type!=ix::WebSocketMessageType::Message)return;
        try{
            if(msg->binary||msg->str.size()>1024*1024)throw std::runtime_error("Invalid request size or type");
            const auto now=Clock::now();if(now-client->window>std::chrono::seconds(1)){client->window=now;client->requests=0;}
            if(++client->requests>40)throw std::runtime_error("Request rate exceeded");
            const auto wire=Json::parse(msg->str);
            if(!client->authenticated){
                {std::lock_guard lock(mutex);
                    if(pairingUsed||Clock::now()>pairingDeadline)throw std::runtime_error("Pairing QR expired or already used; generate a new QR on the PC");
                    client->auth.authenticate(wire);client->authenticated=true;pairingUsed=true;state.token.clear();state.pairingSeconds=0;
                    state.peers.push_back({client->id,client->auth.device(),client->address});}
                socket.sendText(client->auth.send({{"type","authenticated"},{"server_name",state.host},{"version",1}}).dump());
                log("Info","Device authenticated: "+client->auth.device()+" ("+client->address+")");return;
            }
            auto request=client->auth.receive(wire);const auto id=request.at("id");Json response{{"id",id}};
            try{
                const auto tool=request.at("tool").get<std::string>();const auto args=request.value("arguments",Json::object());
                if(tool=="gallery_list")response["result"]=client->gallery.list(output());
                else if(tool=="preview_live"||tool=="preview_capture"||tool=="gallery_image"){
                    // Pace jittery arrivals instead of turning a harmless timing difference
                    // into a failed stream. Each connection has its own network thread.
                    std::this_thread::sleep_until(client->lastImage+std::chrono::milliseconds(100));
                    client->lastImage=Clock::now();std::string jpeg;std::string revision;
                    if(tool=="gallery_image")jpeg=client->gallery.image(output(),args.at("id").get<std::string>());
                    else {
                        const auto source=tool=="preview_capture"?"capture":args.value("processed",false)?"live":"raw_live";
                        auto packet=control.request(Json{{"method","remote_preview"},{"params",{{"source",source}}}}.dump());
                        const auto newline=packet.find('\n');if(newline==std::string::npos)throw std::runtime_error("Missing preview metadata");
                        const auto metadata=Json::parse(packet.substr(0,newline));revision=metadata.at("revision").dump();
                        if(metadata.value("format",std::string{})=="jpeg"){LiveFrame frame;const auto crop=metadata.at("crop");frame.previewRegion.x=crop.at(0);frame.previewRegion.y=crop.at(1);frame.previewRegion.width=crop.at(2);frame.previewRegion.height=crop.at(3);jpeg=cameraJpeg(packet.substr(newline+1),frame);}
                        else {PreviewPixels pixels;pixels.width=metadata.at("width");pixels.height=metadata.at("height");pixels.rgba.assign(packet.begin()+newline+1,packet.end());jpeg=previewJpeg(std::move(pixels));}
                    }
                    if(socket.bufferedAmount()>4*1024*1024)throw std::runtime_error("Slow client: preview skipped");
                    socket.sendBinary(client->auth.sendImage({{"id",id},{"mime_type","image/jpeg"},{"revision",revision}},jpeg));return;
                }
                else {
                    mcp::Protocol protocol([this](const auto& method,const auto& params){return call(method,params);},[]{return Json{{"running",true}};});
                    if(tool=="tools")response["result"]=protocol.tools();
                    else {
                        auto reply=protocol.dispatch({{"jsonrpc","2.0"},{"id",id},{"method","tools/call"},{"params",{{"name",tool},{"arguments",args}}}});
                        if(!reply)throw std::runtime_error("Missing command response");
                        if(reply->contains("error"))throw std::runtime_error((*reply)["error"].at("message").get<std::string>());
                        auto result=reply->at("result");
                        if(result.value("isError",false))throw std::runtime_error(result.at("content").at(0).at("text").get<std::string>());
                        response["result"]=result.contains("structuredContent")?result["structuredContent"]:result;
                    }
                }
            }catch(const std::exception& e){response["error"]=localizeMessage(e.what(),"en");log("Warning",std::string("Request failed: ")+e.what());}
            if(socket.bufferedAmount()>16*1024*1024)throw std::runtime_error("Client cannot keep up with preview stream");
            socket.sendText(client->auth.send(response).dump());
        }catch(const std::exception& e){log("Warning",std::string("Connection rejected: ")+e.what());socket.close(1008,"Invalid or unauthorized request");}
    }
    void run(){
        ix::initNetSystem();std::unique_ptr<ix::WebSocketServer> server;unsigned revision{};int active{};
        while(!quit){
            const int port=desired.load();const auto next=generation.load();
            if(active!=port||revision!=next){
                {std::lock_guard lock(mutex);state.busy=true;}
                if(server){server->stop();server.reset();log("Info","WebSocket server stopped");}
                {std::lock_guard lock(mutex);state.running=false;state.peers.clear();clients.clear();state.token.clear();state.addresses.clear();}
                active=port;revision=next;
                if(port)try{
                    const auto key=randomSecret();server=std::make_unique<ix::WebSocketServer>(port,"0.0.0.0",4,4,5,AF_INET,15);
                    server->disablePerMessageDeflate();server->setOnClientMessageCallback([this,key](auto connection,auto& socket,const auto& msg){message(connection,socket,msg,key);});
                    auto result=server->listen();if(!result.first)throw std::runtime_error(result.second);
                    std::vector<std::pair<int,std::string>> ranked;ULONG bufferSize=16384;std::vector<unsigned char> adapters(bufferSize);ULONG adapterResult{};
                    for(int attempt=0;attempt<3;++attempt){adapterResult=GetAdaptersAddresses(AF_INET,GAA_FLAG_INCLUDE_GATEWAYS,nullptr,reinterpret_cast<IP_ADAPTER_ADDRESSES*>(adapters.data()),&bufferSize);if(adapterResult!=ERROR_BUFFER_OVERFLOW)break;adapters.resize(bufferSize);}
                    if(adapterResult!=NO_ERROR)throw std::runtime_error("Cannot enumerate local network interfaces");
                    for(auto* adapter=reinterpret_cast<IP_ADAPTER_ADDRESSES*>(adapters.data());adapter;adapter=adapter->Next){
                        if(adapter->OperStatus!=IfOperStatusUp||adapter->IfType==IF_TYPE_SOFTWARE_LOOPBACK)continue;
                        for(auto* entry=adapter->FirstUnicastAddress;entry;entry=entry->Next){
                            if(entry->Address.lpSockaddr->sa_family!=AF_INET)continue;const auto* address=&reinterpret_cast<sockaddr_in*>(entry->Address.lpSockaddr)->sin_addr;
                            const auto ipValue=ntohl(address->s_addr);if(!((ipValue>>24)==10||(ipValue>>20)==0xac1||(ipValue>>16)==0xc0a8))continue;
                            char ip[INET_ADDRSTRLEN]{};inet_ntop(AF_INET,address,ip,sizeof(ip));ranked.emplace_back(adapter->IfType==IF_TYPE_IEEE80211?0:adapter->FirstGatewayAddress?1:2,"ws://"+std::string(ip)+":"+std::to_string(port)+"/astrophoto");
                        }
                    }
                    std::sort(ranked.begin(),ranked.end());std::vector<std::string> addresses;for(const auto& [rank,address]:ranked){(void)rank;addresses.push_back(address);}
                    {std::lock_guard lock(mutex);state.port=port;state.token=key;state.addresses=std::move(addresses);state.running=true;state.error.clear();pairingUsed=false;pairingDeadline=Clock::now()+std::chrono::seconds(120);state.pairingSeconds=120;}
                    server->start();std::ofstream file(settings);file<<Json{{"port",port}}.dump(2);if(!file)log("Warning","Could not save server port");
                    log("Info","WebSocket server listening on port "+std::to_string(port));
                }catch(const std::exception& e){if(server){server->stop();server.reset();}{std::lock_guard lock(mutex);state.running=false;state.token.clear();state.error=e.what();}log("Error",e.what());}
                {std::lock_guard lock(mutex);state.busy=false;}
            }
            {std::lock_guard lock(mutex);state.pairingSeconds=state.running&&!pairingUsed?std::max(0,int(std::chrono::duration_cast<std::chrono::seconds>(pairingDeadline-Clock::now()).count())):0;if(!state.pairingSeconds)state.token.clear();}
            if(server)for(const auto& socket:server->getClients()){
                bool expired=false;{std::lock_guard lock(mutex);auto it=clients.find(socket.get());expired=it!=clients.end()&&!it->second->authenticated&&Clock::now()-it->second->opened>std::chrono::seconds(5);}
                if(expired)socket->close(1008,"Authentication timeout");
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        if(server)server->stop();ix::uninitNetSystem();
    }
};
Server::Server(ControlQueue& queue,const std::filesystem::path& settings):impl_(std::make_unique<Impl>(queue,settings)){}
Server::~Server()=default;
void Server::start(int port){if(port<1024||port>65535)return;impl_->desired=port;++impl_->generation;}
void Server::stop(){impl_->desired=0;++impl_->generation;}
void Server::rotateSecret(){++impl_->generation;}
RemoteStatus Server::status()const{std::lock_guard lock(impl_->mutex);return impl_->state;}
}
