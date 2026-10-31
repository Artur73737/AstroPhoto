#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
#include <astro/RemoteAuth.hpp>
#include <stdexcept>
#include <vector>
#include <algorithm>
namespace astro::remote {
namespace {
std::string hex(const unsigned char* bytes,std::size_t size){std::string result;const char* digits="0123456789abcdef";for(std::size_t i=0;i<size;++i){result+=digits[bytes[i]>>4];result+=digits[bytes[i]&15];}return result;}
void check(NTSTATUS status){if(status<0)throw std::runtime_error("Windows cryptography operation failed");}
}
std::string randomSecret(){unsigned char bytes[32];check(BCryptGenRandom(nullptr,bytes,sizeof(bytes),BCRYPT_USE_SYSTEM_PREFERRED_RNG));return hex(bytes,sizeof(bytes));}
std::string hmac(const std::string& key,const std::string& text){
    BCRYPT_ALG_HANDLE algorithm{};check(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,BCRYPT_ALG_HANDLE_HMAC_FLAG));
    struct Algorithm{BCRYPT_ALG_HANDLE h;~Algorithm(){BCryptCloseAlgorithmProvider(h,0);}} guard{algorithm};
    BCRYPT_HASH_HANDLE hash{};check(BCryptCreateHash(algorithm,&hash,nullptr,0,(PUCHAR)key.data(),static_cast<ULONG>(key.size()),0));
    struct Hash{BCRYPT_HASH_HANDLE h;~Hash(){BCryptDestroyHash(h);}} hashGuard{hash};
    check(BCryptHashData(hash,(PUCHAR)text.data(),static_cast<ULONG>(text.size()),0));unsigned char digest[32];check(BCryptFinishHash(hash,digest,sizeof(digest),0));return hex(digest,sizeof(digest));
}
bool constantEqual(const std::string& a,const std::string& b){if(a.size()!=b.size())return false;unsigned char difference{};for(std::size_t i=0;i<a.size();++i)difference|=a[i]^b[i];return difference==0;}
Session::Session(std::string key):key_(std::move(key)),nonce_(randomSecret()){}
nlohmann::json Session::challenge(const std::string& host)const{return {{"type","challenge"},{"version",1},{"nonce",nonce_},{"server_name",host}};}
void Session::authenticate(const nlohmann::json& request){
    if(authenticated_||request.value("type",std::string{})!="authenticate")throw std::runtime_error("Authentication required");
    const auto name=request.at("device_name").get<std::string>();
    if(name.empty()||name.size()>96||std::any_of(name.begin(),name.end(),[](unsigned char c){return c<32||c==127;}))throw std::runtime_error("Invalid device name");
    if(!constantEqual(request.at("proof").get<std::string>(),hmac(key_,"auth\n"+nonce_+"\n"+name)))throw std::runtime_error("Authentication failed");
    device_=name;authenticated_=true;
}
nlohmann::json Session::receive(const nlohmann::json& wire){
    if(!authenticated_)throw std::runtime_error("Authentication required");
    const auto sequence=wire.at("seq").get<std::uint64_t>();const auto payload=wire.at("payload").get<std::string>();
    if(sequence!=received_+1||payload.size()>1024*1024||!constantEqual(wire.at("mac").get<std::string>(),hmac(key_,"client\n"+nonce_+"\n"+std::to_string(sequence)+"\n"+payload)))throw std::runtime_error("Invalid signature or replayed request");
    auto result=nlohmann::json::parse(payload);++received_;return result;
}
nlohmann::json Session::send(const nlohmann::json& value){
    if(!authenticated_)throw std::runtime_error("Authentication required");const auto payload=value.dump();const auto sequence=++sent_;
    return {{"seq",sequence},{"payload",payload},{"mac",hmac(key_,"server\n"+nonce_+"\n"+std::to_string(sequence)+"\n"+payload)}};
}
std::string Session::sendImage(const nlohmann::json& metadata,const std::string& jpeg){
    if(!authenticated_)throw std::runtime_error("Authentication required");
    if(jpeg.size()>4*1024*1024)throw std::runtime_error("Preview exceeds network budget");
    const auto payload=metadata.dump();const auto sequence=++sent_;
    const auto header=nlohmann::json{{"seq",sequence},{"payload",payload},{"mac",hmac(key_,"server-image\n"+nonce_+"\n"+std::to_string(sequence)+"\n"+payload+"\n"+jpeg)}}.dump();
    const auto size=static_cast<std::uint32_t>(header.size());std::string packet;
    for(int shift:{24,16,8,0})packet.push_back(static_cast<char>(size>>shift));
    packet+=header;packet+=jpeg;return packet;
}
}
