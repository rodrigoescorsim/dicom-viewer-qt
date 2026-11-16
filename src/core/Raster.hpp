#pragma once

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

namespace dv {

/// Tightly packed raster: row y starts at `data[y * width * channels]`, with no row padding.
template <typename T>
    requires std::is_unsigned_v<T>
struct Raster {
    int width{0};
    int height{0};
    int channels{1};
    std::vector<T> data;

    [[nodiscard]] std::size_t rowLength() const noexcept {
        return static_cast<std::size_t>(width) * static_cast<std::size_t>(channels);
    }
    [[nodiscard]] std::span<const T> row(int y) const noexcept {
        return std::span<const T>(data).subspan(static_cast<std::size_t>(y) * rowLength(), rowLength());
    }
    [[nodiscard]] bool isValid() const noexcept {
        return width > 0 && height > 0 && channels > 0 &&
               data.size() == rowLength() * static_cast<std::size_t>(height);
    }
};

using GrayRaster = Raster<std::uint16_t>; ///< 1 channel of 16-bit indices.
using RgbRaster = Raster<std::uint8_t>;   ///< 3 channels, interleaved R, G, B.

/// Halves the resolution with a 2x2 box filter (rounded mean per channel).
/// Output size is ceil(w/2) x ceil(h/2); on odd sizes the last column/row is replicated.
template <typename T>
[[nodiscard]] Raster<T> downsample2x(const Raster<T>& src) {
    assert(src.isValid());
    Raster<T> dst;
    dst.width = (src.width + 1) / 2;
    dst.height = (src.height + 1) / 2;
    dst.channels = src.channels;
    dst.data.resize(dst.rowLength() * static_cast<std::size_t>(dst.height));

    const auto channels = static_cast<std::size_t>(src.channels);
    T* out = dst.data.data();
    for (int y = 0; y < dst.height; ++y) {
        const T* row0 = src.row(2 * y).data();
        const T* row1 = src.row(std::min(2 * y + 1, src.height - 1)).data();
        for (int x = 0; x < dst.width; ++x) {
            const std::size_t x0 = static_cast<std::size_t>(2 * x) * channels;
            const std::size_t x1 = static_cast<std::size_t>(std::min(2 * x + 1, src.width - 1)) * channels;
            for (std::size_t c = 0; c < channels; ++c) {
                const std::uint32_t sum = std::uint32_t{row0[x0 + c]} + std::uint32_t{row0[x1 + c]} +
                                          std::uint32_t{row1[x0 + c]} + std::uint32_t{row1[x1 + c]};
                *out++ = static_cast<T>((sum + 2U) / 4U);
            }
        }
    }
    return dst;
}

/// A pyramid stops growing once either side of its last level is <= this value.
inline constexpr int kMinPyramidSide = 256;

/// Returns {level0, level0 / 2, level0 / 4, ...}. A level is appended while both sides of the last one
/// exceed `minSide`. The result always contains at least `level0`.
template <typename T>
[[nodiscard]] std::vector<Raster<T>> buildPyramid(Raster<T> level0, int minSide = kMinPyramidSide) {
    assert(level0.isValid());
    std::vector<Raster<T>> levels;
    levels.push_back(std::move(level0));
    while (levels.back().width > minSide && levels.back().height > minSide) {
        levels.push_back(downsample2x(levels.back()));
    }
    return levels;
}

/// Pyramid level to draw at `scale` device pixels per image pixel: the coarsest level that still has at
/// least one texel per device pixel, i.e. floor(log2(1 / scale)) clamped to [0, levelCount - 1].
[[nodiscard]] inline int selectPyramidLevel(double scale, int levelCount) noexcept {
    if (levelCount <= 1 || !(scale > 0.0) || scale >= 1.0) {
        return 0;
    }
    const int level = static_cast<int>(std::floor(std::log2(1.0 / scale)));
    return std::clamp(level, 0, levelCount - 1);
}

} // namespace dv
