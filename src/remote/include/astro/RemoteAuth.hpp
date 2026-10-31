#pragma once
#include <nlohmann/json.hpp>
#include <string>
#include <cstdint>
namespace astro::remote {
std::string randomSecret();
std::string hmac(const std::string& key,const std::string& text);
bool constantEqual(const std::string&,const std::string&);
// A new random nonce binds every signed packet to one connection. Sequence
// numbers reject replay; direction separation prevents reflected responses.
class Session {
public:
    explicit Session(std::string key);
    nlohmann::json challenge(const std::string& host)const;
    void authenticate(const nlohmann::json&);
    nlohmann::json receive(const nlohmann::json&);
    nlohmann::json send(const nlohmann::json&);
    std::string sendImage(const nlohmann::json& metadata,const std::string& jpeg);
    bool authenticated()const{return authenticated_;}
    const std::string& device()const{return device_;}
private:
    std::string key_,nonce_,device_;bool authenticated_{};
    std::uint64_t received_{},sent_{};
};
}
