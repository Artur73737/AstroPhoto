#pragma once
#include "ControlQueue.hpp"
#include <memory>
#include <string>
namespace astro::mcp {
class Server {
public:
    Server(ControlQueue&,const std::string& transport);
    ~Server();
    bool finished() const;
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
class InstanceGuard {
public:
    InstanceGuard();~InstanceGuard();
private:void* handle_{};
};
void initializeSettings();
std::string configuredTransport();
}
