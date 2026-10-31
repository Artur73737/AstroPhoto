#pragma once
#include <astro/RemoteState.hpp>
#include <memory>
#include <filesystem>
namespace astro {class ControlQueue;}
namespace astro::remote {
class Server {
public:
    Server(ControlQueue&,const std::filesystem::path& settings);
    ~Server();
    void start(int port);void stop();void rotateSecret();
    RemoteStatus status()const;
private:struct Impl;std::unique_ptr<Impl> impl_;
};
}
