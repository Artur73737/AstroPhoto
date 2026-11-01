#pragma once
#include <astro/Core.hpp>
namespace astro {
class LiveStackRun {
public:
    LiveStackRun(ICamera&,IFileSystem&,IClock&,IStackProcessor&,Log);
    void start(const Config&,const std::string& metadata);
    void tick();
    void stop(); // graceful: finish the current exposure and download
    void fail(const std::string&);
    const StackStatus& status() const {return status_;}
private:
    void finalize(const std::string&);
    ICamera& camera_;IFileSystem& files_;IClock& clock_;IStackProcessor& processor_;Log log_;
    Config config_;SequenceItem item_;StackStatus status_;std::map<Property,std::uint32_t> frozen_;
    SteadyTime started_{},exposureStart_{},deadline_{},next_{};
    bool shutter_{},failed_{};
};
}
