#pragma once
#include <memory>
namespace astro {
// Presentation-only GPU pass. Never modifies source textures, RAWs or remote previews.
class NightDisplay {
public:
    NightDisplay();
    ~NightDisplay();
    void render(int width,int height,bool enabled);
    void caption(void* window,bool light,bool night);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
