#include "FocusPeaking.hpp"
#include "StarAnalysis.hpp"
#include "LiveViewPreview.hpp"
#include "ImageAnalysis.hpp"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <astro/WorkerWait.hpp>
#include <condition_variable>

// Replay a real recorded EVF JPEG; never opens the camera or generates frames.
template<class F> void measure(const char* name,F action) {
    action();std::vector<double> elapsed;
    for(int i=0;i<30;++i){auto start=std::chrono::steady_clock::now();action();elapsed.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());}
    std::sort(elapsed.begin(),elapsed.end());
    std::cout<<name<<" median_ms="<<elapsed[15]<<" p95_ms="<<elapsed[28]<<'\n';
}
int main(int argc,char** argv) {
    if(argc!=2){std::cerr<<"Usage: astro_live_benchmark recorded-live-frame.jpg | --timers\n";return 1;}
    try {
        if(std::string(argv[1])=="--timers"){
            std::mutex mutex;std::condition_variable_any cv;std::stop_source source;astro::WorkerWait timer;
            measure("condition_variable_1ms",[&]{std::unique_lock lock(mutex);cv.wait_for(lock,source.get_token(),std::chrono::milliseconds(1),[]{return false;});});
            measure("worker_timer_1ms",[&]{timer.wait(source.get_token(),std::chrono::milliseconds(1));});return 0;
        }
        auto pixels=astro::decodePreview(astro::pathFromUtf8(argv[1]));
        // decodePreview initializes COM only for its call; use it for repeatable
        // file decode timings, and isolate the CPU stages from file I/O below.
        std::cout<<"frame="<<pixels.width<<'x'<<pixels.height<<'\n';
        measure("decode",[&]{auto p=astro::decodePreview(astro::pathFromUtf8(argv[1]));if(p.rgba.empty())throw std::runtime_error("No pixels");});
        measure("stars",[&]{auto field=astro::measureStars(pixels,5);(void)field;});
        measure("star_peaking",[&]{auto display=astro::focusPeaking(pixels,35,true);if(display.rgba.empty())throw std::runtime_error("No peaking");});
        measure("histogram",[&]{auto histogram=astro::imageHistogram(pixels);(void)histogram;});
        measure("preview_transfer",[&]{auto input=pixels;astro::LiveFrame frame;auto display=astro::prepareLiveViewPreview(std::move(input),frame);if(display.rgba.empty())throw std::runtime_error("No preview");});
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
