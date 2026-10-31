#pragma once
#include <string>
#include <vector>
namespace astro {
struct RemotePeer {std::string id,name,address;};
struct RemoteLog {std::string time,level,message;};
struct RemoteStatus {
    bool running{},busy{};int port{47654};
    std::string host,token,error;
    std::vector<std::string> addresses;
    std::vector<RemotePeer> peers;
    std::vector<RemoteLog> logs;
    int pairingSeconds{};
};
}
