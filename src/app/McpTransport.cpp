#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <httplib.h>
#include <windows.h>
#include <shlobj.h>
#include <bcrypt.h>
#include "McpTransport.hpp"
#include "McpProtocol.hpp"
#include <filesystem>
#include <fstream>
#include <thread>
#include <atomic>
namespace astro::mcp {
namespace {
struct Handle {HANDLE value{};~Handle(){if(value&&value!=INVALID_HANDLE_VALUE)CloseHandle(value);}};
std::filesystem::path settingsPath(){
    PWSTR path{};if(FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData,0,nullptr,&path)))throw std::runtime_error("Local app data folder unavailable");
    auto result=std::filesystem::path(path)/L"AstroPhoto"/L"MCP"/L"settings.json";CoTaskMemFree(path);return result;
}
Json readSettings(){
    const auto path=settingsPath();if(!std::filesystem::exists(path)){
        std::filesystem::create_directories(path.parent_path());unsigned char random[32];if(BCryptGenRandom(nullptr,random,sizeof(random),BCRYPT_USE_SYSTEM_PREFERRED_RNG)!=0)throw std::runtime_error("Cannot generate MCP bearer token");
        std::string token;const char* hex="0123456789abcdef";for(auto c:random){token+=hex[c>>4];token+=hex[c&15];}
        const auto text=Json{{"transport","http"},{"port",47653},{"token",token}}.dump(2);
        Handle file{CreateFileW(path.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr)};
        if(file.value!=INVALID_HANDLE_VALUE){DWORD written{};if(!WriteFile(file.value,text.data(),static_cast<DWORD>(text.size()),&written,nullptr)||written!=text.size())throw std::runtime_error("Cannot write MCP settings");}
        else if(GetLastError()!=ERROR_FILE_EXISTS)throw std::runtime_error("Cannot create MCP settings");
    }
    std::ifstream in(path);Json settings;in>>settings;
    const auto port=settings.at("port").get<int>();const auto token=settings.at("token").get<std::string>();
    if(port<1024||port>65535||token.size()<32||token.size()>256||std::any_of(token.begin(),token.end(),[](unsigned char c){return c<33||c>126;}))throw std::runtime_error("Invalid MCP port/token configuration");
    return settings;
}
bool tokenMatches(const std::string& a,const std::string& b){if(a.size()!=b.size())return false;unsigned char difference{};for(std::size_t i=0;i<a.size();++i)difference|=static_cast<unsigned char>(a[i]^b[i]);return difference==0;}
void writeHandle(HANDLE handle,const std::string& text){std::size_t offset{};while(offset<text.size()){DWORD count{};if(!WriteFile(handle,text.data()+offset,static_cast<DWORD>(std::min<std::size_t>(text.size()-offset,65536)),&count,nullptr)||!count)throw std::runtime_error("MCP output closed");offset+=count;}}
// Each accepted HTTP request executes on the MCP listener thread, without a pool.
struct InlineQueue final:httplib::TaskQueue{bool enqueue(std::function<void()> job)override{job();return true;}void shutdown()override{}};
}
void initializeSettings(){(void)readSettings();}
std::string configuredTransport(){const auto mode=readSettings().value("transport",std::string("http"));if(mode!="http"&&mode!="stdio")throw std::runtime_error("MCP transport must be http or stdio");return mode;}
InstanceGuard::InstanceGuard(){
    handle_=CreateMutexW(nullptr,FALSE,L"Local\\AstroPhoto.SingleInstance");
    if(!handle_)throw std::runtime_error("Unable to claim application instance");
    if(GetLastError()==ERROR_ALREADY_EXISTS){CloseHandle(handle_);handle_=nullptr;throw std::runtime_error("AstroPhoto is already open. Close it before selecting a different MCP transport.");}
}
InstanceGuard::~InstanceGuard(){if(handle_)CloseHandle(handle_);}
struct Server::Impl {
    ControlQueue& queue;std::string mode;httplib::Server http;std::atomic<bool> stopping{},done{};Protocol protocol;std::thread thread;
    Impl(ControlQueue& q,std::string transport):queue(q),mode(std::move(transport)),protocol(
        [this](const std::string& method,const Json& p){auto result=Json::parse(queue.request(Json{{"method",method},{"params",p}}.dump()));if(method=="state"){result["threads"]["mcp"]=std::hash<std::thread::id>{}(std::this_thread::get_id());result["transport"]=mode;result["pid"]=GetCurrentProcessId();}return result;},
        [](){return Json{{"running",true},{"reused",true},{"note","MCP runs inside this UI process; no additional application is launched"}};}){
        if(mode!="http"&&mode!="stdio")throw std::invalid_argument("Choose exactly one MCP transport: http or stdio");
        if(mode=="stdio"){
            const auto input=GetStdHandle(STD_INPUT_HANDLE),output=GetStdHandle(STD_OUTPUT_HANDLE);
            if(!input||input==INVALID_HANDLE_VALUE||!output||output==INVALID_HANDLE_VALUE||GetFileType(input)!=FILE_TYPE_PIPE)throw std::runtime_error("stdio requires redirected stdin/stdout: launch from Codex");
            thread=std::thread([this]{stdioLoop();done=true;});return;
        }
        const auto settings=readSettings();const auto port=settings.at("port").get<int>();const auto authorization="Bearer "+settings.at("token").get<std::string>();
        http.set_payload_max_length(1024*1024);http.set_read_timeout(1);http.set_write_timeout(3);http.set_keep_alive_max_count(1);
        http.new_task_queue=[](){return new InlineQueue;};
        http.set_pre_routing_handler([authorization,port](const httplib::Request& req,httplib::Response& res){
            const auto host=req.get_header_value("Host"),origin=req.get_header_value("Origin");const auto ip="127.0.0.1:"+std::to_string(port),local="localhost:"+std::to_string(port);
            res.set_header("Cache-Control","no-store");
            if((host!=ip&&host!=local)||(!origin.empty()&&origin!="http://"+ip&&origin!="http://"+local)){res.status=403;res.set_content("Forbidden origin/host","text/plain");return httplib::Server::HandlerResponse::Handled;}
            if(!tokenMatches(req.get_header_value("Authorization"),authorization)){res.status=401;res.set_content("Unauthorized","text/plain");return httplib::Server::HandlerResponse::Handled;}
            auto version=req.get_header_value("MCP-Protocol-Version");if(!version.empty()&&version!="2025-11-25"&&version!="2025-06-18"&&version!="2025-03-26"&&version!="2024-11-05"){res.status=400;res.set_content("Unsupported MCP protocol version","text/plain");return httplib::Server::HandlerResponse::Handled;}
            return httplib::Server::HandlerResponse::Unhandled;
        });
        http.Post("/mcp",[this](const httplib::Request& req,httplib::Response& res){
            const auto accept=req.get_header_value("Accept");if(accept.find("application/json")==std::string::npos||accept.find("text/event-stream")==std::string::npos){res.status=406;return;}
            if(!req.get_header_value("Content-Type").starts_with("application/json")){res.status=415;return;}
            try{auto result=protocol.dispatch(Json::parse(req.body));if(result)res.set_content(result->dump(),"application/json");else res.status=202;}
            catch(const Json::parse_error&){res.status=400;res.set_content(error(nullptr,-32700,"Invalid JSON").dump(),"application/json");}
            catch(const std::exception&){res.status=500;res.set_content(error(nullptr,-32603,"MCP server error").dump(),"application/json");}
        });
        auto unsupported=[](const httplib::Request&,httplib::Response& res){res.status=405;res.set_header("Allow","POST");};http.Get("/mcp",unsupported);http.Delete("/mcp",unsupported);
        if(!http.bind_to_port("127.0.0.1",port))throw std::runtime_error("MCP HTTP port unavailable: "+std::to_string(port));
        thread=std::thread([this]{http.listen_after_bind();done=true;});
    }
    ~Impl(){stopping=true;queue.close();http.stop();if(thread.joinable()){CancelSynchronousIo(thread.native_handle());thread.join();}}
    void stdioLoop(){
        const auto input=GetStdHandle(STD_INPUT_HANDLE),output=GetStdHandle(STD_OUTPUT_HANDLE);
        std::string line;bool oversized{};char buffer[4096];DWORD read{};
        try {
            while(!stopping){DWORD available{};if(!PeekNamedPipe(input,nullptr,0,nullptr,&available,nullptr))break;if(!available){std::this_thread::sleep_for(std::chrono::milliseconds(10));continue;}
                if(!ReadFile(input,buffer,std::min<DWORD>(available,sizeof(buffer)),&read,nullptr)||!read)break;
                for(DWORD i=0;i<read;++i){
                    if(buffer[i]=='\n'){
                        if(oversized)writeHandle(output,error(nullptr,-32600,"Request exceeds 1 MiB").dump()+"\n");
                        else if(!line.empty()){
                            try{auto reply=protocol.dispatch(Json::parse(line));if(reply)writeHandle(output,reply->dump()+"\n");}
                            catch(const Json::parse_error&){writeHandle(output,error(nullptr,-32700,"Invalid JSON").dump()+"\n");}
                        }
                        line.clear();oversized=false;
                    }else if(!oversized){line+=buffer[i];if(line.size()>1024*1024){line.clear();oversized=true;}}
                }
            }
        }catch(const std::exception& e){try{writeHandle(GetStdHandle(STD_ERROR_HANDLE),std::string(e.what())+"\n");}catch(...){}}
    }
};
Server::Server(ControlQueue& queue,const std::string& mode):impl_(std::make_unique<Impl>(queue,mode)){}
Server::~Server()=default;
bool Server::finished()const{return impl_->done;}
}
