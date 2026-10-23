#pragma once
#include "PhotoPreview.hpp"
#include <astro/PreviewAnalysis.hpp>
namespace astro {
Histogram imageHistogram(const PreviewPixels&);
FocusReading measureFocus(const PreviewPixels&,FocusTarget,const PreviewPixels* peaking=nullptr);
class FocusTracker {
public:
    FocusReading update(FocusReading,std::uint64_t frame,std::uint64_t context);
    void reset();
private:
    FocusReading reading_;
    std::uint64_t frame_{},context_{};
    double smooth_{},minimum_{},best_{};
    int samples_{},stable_{},declining_{};
    bool bracketed_{};
};
}
