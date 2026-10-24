#include "ControlQueue.hpp"
#include <stdexcept>
namespace astro {
std::string ControlQueue::request(std::string text,std::chrono::milliseconds timeout){
    if(timeout<=std::chrono::milliseconds::zero())throw std::invalid_argument("Invalid request timeout");
    auto r=std::make_shared<Request>();r->text=std::move(text);r->deadline=std::chrono::steady_clock::now()+timeout;auto future=r->reply.get_future();
    {std::lock_guard lock(mutex_);if(closed_)throw std::runtime_error("Application is closing");if(pending_.size()>=capacity_)throw std::runtime_error("UI control queue is full");pending_.push_back(r);}
    if(future.wait_until(r->deadline)!=std::future_status::ready){
        std::lock_guard lock(mutex_);r->cancelled=true;
        if(r->claimed)throw std::runtime_error("UI request timed out after dispatch; inspect state before repeating a mutation");
        throw std::runtime_error("UI request expired before dispatch; no action executed");
    }
    return future.get();
}
void ControlQueue::pump(const std::function<std::string(const std::string&)>& handler){
    // Bound work per rendered frame. All state mutations and GL snapshots stay on UI.
    for(int i=0;i<4;++i){
        std::shared_ptr<Request> r;
        {std::lock_guard lock(mutex_);if(closed_||pending_.empty())return;r=pending_.front();pending_.pop_front();if(r->cancelled||std::chrono::steady_clock::now()>=r->deadline)continue;r->claimed=true;}
        try{r->reply.set_value(handler(r->text));}catch(...){r->reply.set_exception(std::current_exception());}
    }
}
void ControlQueue::close(){
    std::deque<std::shared_ptr<Request>> pending;
    {std::lock_guard lock(mutex_);closed_=true;pending.swap(pending_);}
    for(auto& r:pending)r->reply.set_exception(std::make_exception_ptr(std::runtime_error("Application is closing")));
}
}
