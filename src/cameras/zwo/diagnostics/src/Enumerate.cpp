#include <astro/cameras/zwo/sdk/ZwoCamera.hpp>
#include <iostream>
int main() {
    try {
        auto devices = astro::zwo::Camera::enumerate();
        std::cout << "Detected real ASI cameras: " << devices.size() << '\n';
        for (auto& d : devices)
            std::cout << d.id << " " << d.name << " " << d.width << "x" << d.height << '\n';
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
