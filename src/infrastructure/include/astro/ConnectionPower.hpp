#pragma once
#include <astro/Core.hpp>
namespace astro {
// Owned by the camera worker. The OS releases the handle even on process exit.
class ConnectionPower {
public:
    explicit ConnectionPower(Log log):log_(std::move(log)){}
    ~ConnectionPower();
    ConnectionPower(const ConnectionPower&)=delete;
    ConnectionPower& operator=(const ConnectionPower&)=delete;
    void setConnected(bool connected);
    bool active() const {return handle_!=nullptr;}
private:
    void release() noexcept;
    void* handle_{};
    Log log_;
};
}
