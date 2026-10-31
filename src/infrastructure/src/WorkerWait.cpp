#include <astro/WorkerWait.hpp>
#include <system_error>
#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
namespace astro {
struct WorkerWait::Impl {
    struct Handle {HANDLE value{};~Handle(){if(value)CloseHandle(value);}} timer,stopped;
    static void check(bool ok){if(!ok)throw std::system_error(static_cast<int>(GetLastError()),std::system_category(),"Camera worker timer");}
    Impl(){
        // Windows 10 1803+; older systems retain a cancellable normal timer.
        timer.value=CreateWaitableTimerExW(nullptr,nullptr,CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,TIMER_ALL_ACCESS);
        if(!timer.value&&GetLastError()==ERROR_INVALID_PARAMETER)timer.value=CreateWaitableTimerExW(nullptr,nullptr,0,TIMER_ALL_ACCESS);
        check(timer.value!=nullptr);stopped.value=CreateEventW(nullptr,TRUE,FALSE,nullptr);check(stopped.value!=nullptr);
    }
};
WorkerWait::WorkerWait():impl_(std::make_unique<Impl>()){}
WorkerWait::~WorkerWait()=default;
bool WorkerWait::wait(std::stop_token stop,std::chrono::milliseconds duration){
    if(stop.stop_requested())return false;
    if(duration.count()<=0)return true;
    Impl::check(ResetEvent(impl_->stopped.value)!=FALSE);
    std::stop_callback callback(stop,[this]{SetEvent(impl_->stopped.value);});
    LARGE_INTEGER due;due.QuadPart=-static_cast<LONGLONG>(duration.count())*10000;
    Impl::check(SetWaitableTimer(impl_->timer.value,&due,0,nullptr,nullptr,FALSE)!=FALSE);
    const HANDLE handles[]{impl_->stopped.value,impl_->timer.value};
    const auto result=WaitForMultipleObjects(2,handles,FALSE,INFINITE);
    Impl::check(result!=WAIT_FAILED);
    return result==WAIT_OBJECT_0+1&&!stop.stop_requested();
}
}
#else
#include <condition_variable>
#include <mutex>
namespace astro {
struct WorkerWait::Impl {std::mutex mutex;std::condition_variable_any timer;};
WorkerWait::WorkerWait():impl_(std::make_unique<Impl>()){}
WorkerWait::~WorkerWait()=default;
bool WorkerWait::wait(std::stop_token stop,std::chrono::milliseconds duration){
    std::unique_lock lock(impl_->mutex);impl_->timer.wait_for(lock,stop,duration,[]{return false;});return !stop.stop_requested();
}
}
#endif
