#include "core/Voi.hpp"

#include <cmath>
#include <utility>

namespace dv {

VoiWindow sanitized(VoiWindow window) noexcept {
    if (!std::isfinite(window.center)) {
        window.center = 0.0;
    }
    if (!std::isfinite(window.width) || window.width < kMinWindowWidth) {
        window.width = kMinWindowWidth;
    }
    return window;
}

double evaluateVoi(const VoiWindow& window, double x) noexcept {
    const double c = window.center;
    const double w = window.width;
    switch (window.function) {
    case VoiFunction::Linear: {
        // PS3.3 C.11.2.1.2.1
        const double half = (w - 1.0) / 2.0;
        if (x <= c - 0.5 - half) {
            return 0.0;
        }
        if (x > c - 0.5 + half) {
            return 1.0;
        }
        return (x - (c - 0.5)) / (w - 1.0) + 0.5;
    }
    case VoiFunction::LinearExact: {
        // PS3.3 C.11.2.1.3.2
        if (x <= c - w / 2.0) {
            return 0.0;
        }
        if (x > c + w / 2.0) {
            return 1.0;
        }
        return (x - c) / w + 0.5;
    }
    case VoiFunction::Sigmoid:
        // PS3.3 C.11.2.1.3.1
        return 1.0 / (1.0 + std::exp(-4.0 * (x - c) / w));
    }
    return 0.0;
}

VoiWindow fullRangeWindow(double minValue, double maxValue) noexcept {
    if (maxValue < minValue) {
        std::swap(minValue, maxValue);
    }
    return sanitized({.center = (minValue + maxValue + 1.0) / 2.0,
                      .width = maxValue - minValue + 1.0,
                      .function = VoiFunction::Linear});
}

} // namespace dv
