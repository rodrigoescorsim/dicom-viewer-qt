#include "core/GrayLut.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>

namespace dv {

std::vector<std::uint8_t> buildGrayLut(const GrayLutParams& params) {
    assert(params.entryCount >= 1 && params.entryCount <= 65536);
    const VoiWindow window = sanitized(params.window);

    std::vector<std::uint8_t> lut(params.entryCount);
    for (std::uint32_t i = 0; i < params.entryCount; ++i) {
        const double y = std::clamp(evaluateVoi(window, params.mapping.valueOf(i)), 0.0, 1.0);
        const auto gray = static_cast<std::uint8_t>(std::lround(y * 255.0));
        lut[i] = params.inverted ? static_cast<std::uint8_t>(255 - gray) : gray;
    }
    if (params.padding && params.padding->first < params.entryCount) {
        const std::uint32_t last = std::min<std::uint32_t>(params.padding->last, params.entryCount - 1);
        for (std::uint32_t i = params.padding->first; i <= last; ++i) {
            lut[i] = 0;
        }
    }
    return lut;
}

void applyGrayLut(std::span<const std::uint16_t> src, std::span<const std::uint8_t> lut, int width, int height,
                  std::uint8_t* dst, std::ptrdiff_t dstStride) noexcept {
    assert(src.size() == static_cast<std::size_t>(width) * static_cast<std::size_t>(height));
    assert(dstStride >= width);
    const std::uint16_t* in = src.data();
    const std::uint8_t* table = lut.data();
    for (int y = 0; y < height; ++y) {
        std::uint8_t* out = dst + static_cast<std::ptrdiff_t>(y) * dstStride;
        for (int x = 0; x < width; ++x) {
            out[x] = table[in[x]];
        }
        in += width;
    }
}

} // namespace dv
