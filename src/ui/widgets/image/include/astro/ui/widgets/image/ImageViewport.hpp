#pragma once
#include <algorithm>
#include <cmath>
namespace astro::ui {
struct ImageViewport {
    double zoom{1}, cx{.5}, cy{.5};
    void reset() {
        zoom = 1;
        cx = cy = .5;
    }
    double span() const { return 1 / zoom; }
    double left() const { return cx - span() / 2; }
    double top() const { return cy - span() / 2; }
    void clamp() {
        const auto half = span() / 2;
        cx = std::clamp(cx, half, 1 - half);
        cy = std::clamp(cy, half, 1 - half);
    }
    void wheel(double delta, double x, double y) {
        const double px = left() + x * span(), py = top() + y * span();
        zoom = std::clamp(zoom * std::pow(1.2, delta), 1., 32.);
        cx = px + (.5 - x) * span();
        cy = py + (.5 - y) * span();
        clamp();
    }
    void pan(double dx, double dy) {
        cx -= dx * span();
        cy -= dy * span();
        clamp();
    }
};
}  // namespace astro::ui
