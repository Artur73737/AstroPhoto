#pragma once
#include <chrono>
#include <deque>
#include <functional>
#include <future>
#include <mutex>
#include <string>
namespace astro {
// A bounded request/reply channel, not a thread. MCP produces; only UI consumes.
class ControlQueue {
public:
    explicit ControlQueue(std::size_t capacity=32):capacity_(capacity){}
    ~ControlQueue(){close();}
    std::string request(std::string,std::chrono::milliseconds timeout=std::chrono::seconds(5));
    void pump(const std::function<std::string(const std::string&)>&);
    void close();
private:
    struct Request {
        std::string text;std::promise<std::string> reply;
        std::chrono::steady_clock::time_point deadline;bool claimed{},cancelled{};
    };
    std::mutex mutex_;std::deque<std::shared_ptr<Request>> pending_;
    std::size_t capacity_;bool closed_{};
};
}
