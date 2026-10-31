#pragma once
#include <chrono>
#include <memory>
#include <stop_token>
namespace astro {
// Interruptible waits without changing the system-wide timer resolution.
class WorkerWait {
public:
    WorkerWait();
    ~WorkerWait();
    bool wait(std::stop_token,std::chrono::milliseconds);
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
}
