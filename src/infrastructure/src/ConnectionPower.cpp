#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif
#include <astro/ConnectionPower.hpp>
#include <system_error>
namespace astro {
ConnectionPower::~ConnectionPower(){release();}
void ConnectionPower::release() noexcept {
#ifdef _WIN32
    if(!handle_)return;
    for(auto type:{PowerRequestDisplayRequired,PowerRequestSystemRequired})
        if(!PowerClearRequest(handle_,type))try{log_("Warning","PowerClearRequest: "+std::to_string(GetLastError()));}catch(...){}
    if(!CloseHandle(handle_))try{log_("Warning","CloseHandle power request: "+std::to_string(GetLastError()));}catch(...){}
    handle_=nullptr;
#endif
}
void ConnectionPower::setConnected(bool connected){
    if(!connected){release();return;}
    if(active())return;
#ifdef _WIN32
    wchar_t reason[]=L"AstroPhoto: connected camera acquisition and focus";
    REASON_CONTEXT context{};context.Version=POWER_REQUEST_CONTEXT_VERSION;
    context.Flags=POWER_REQUEST_CONTEXT_SIMPLE_STRING;context.Reason.SimpleReasonString=reason;
    auto handle=PowerCreateRequest(&context);
    if(handle==INVALID_HANDLE_VALUE)throw std::system_error(GetLastError(),std::system_category(),"PowerCreateRequest");
    if(!PowerSetRequest(handle,PowerRequestSystemRequired)){
        const auto error=GetLastError();CloseHandle(handle);throw std::system_error(error,std::system_category(),"PowerSetRequest system");
    }
    if(!PowerSetRequest(handle,PowerRequestDisplayRequired)){
        const auto error=GetLastError();PowerClearRequest(handle,PowerRequestSystemRequired);CloseHandle(handle);
        throw std::system_error(error,std::system_category(),"PowerSetRequest display");
    }
    handle_=handle;
    log_("Info","Camera connected: Windows automatic sleep and display timeout inhibited");
#else
    throw std::runtime_error("Connection power requests require Windows");
#endif
}
}
