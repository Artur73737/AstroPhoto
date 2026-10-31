#pragma once
#include <astro/Core.hpp>
namespace astro {
double focusSignatureDistance(const std::vector<double>&,const std::vector<double>&);
struct FocusLimitObservation {std::vector<double> signal;double noise{};};
// Optical inference, not an encoder. A plateau alone never confirms a limit:
// reverse motion and a reproducible return must also be observed.
class FocusHoming {
public:
    FocusHoming(ICamera& camera,IClock& clock):camera_(camera),clock_(clock){}
    void start(const StarFocusOptions&);
    void tick(const StarField* field=nullptr,std::uint64_t revision=0);
    void cancel();void fail(const std::string&);
    const StarFocusProgress& progress()const{return progress_;}
    const FocusLimitObservation& nearLimit()const{return near_;}
    const FocusLimitObservation& farLimit()const{return far_;}
private:
    enum class Mode {Seek,ReverseProbe,ConfirmReturn};
    void beginSamples();void move(int direction,int count);void sample(const StarField&);
    void point(FocusLimitObservation);double threshold(const FocusLimitObservation&,const FocusLimitObservation&)const;
    ICamera& camera_;IClock& clock_;StarFocusOptions options_;StarFocusProgress progress_;
    SteadyTime deadline_{},ready_{},sampleDeadline_{},lastFrameAt_{};std::uint64_t revision_{},moveRevision_{};
    int discard_{},stable_{},legMoves_{},width_{},height_{},recalibrations_{};Mode mode_{Mode::Seek};
    std::deque<int> moves_;std::vector<std::vector<double>> samples_;
    FocusLimitObservation previous_,candidate_,plateau_,near_,far_;
};
}
