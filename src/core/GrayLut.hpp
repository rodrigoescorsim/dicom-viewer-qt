#pragma once

#include "core/Voi.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace dv {

/// Relation between a stored 16-bit index and the modality value it represents:
/// `value = offset + index * scale`. `scale` is 1.0 unless the dynamic range exceeds 16 bits.
struct ValueMapping {
    double offset{0.0};
    double scale{1.0};

    [[nodiscard]] constexpr double valueOf(std::uint32_t index) const noexcept {
        return offset + static_cast<double>(index) * scale;
    }

    friend bool operator==(const ValueMapping&, const ValueMapping&) = default;
};

/// Inclusive range of indices.
struct IndexRange {
    std::uint16_t first{0};
    std::uint16_t last{0};

    friend bool operator==(const IndexRange&, const IndexRange&) = default;
};

struct GrayLutParams {
    VoiWindow window; ///< Sanitized by buildGrayLut.
    ValueMapping mapping;
    std::uint32_t entryCount{1};       ///< Highest index present in the image + 1 (1..65536).
    bool inverted{false};              ///< true for MONOCHROME1.
    std::optional<IndexRange> padding; ///< Indices that are always rendered black.
};

/// Builds the index -> 8-bit gray table: VOI function, then inversion, then padding.
[[nodiscard]] std::vector<std::uint8_t> buildGrayLut(const GrayLutParams& params);

/// Writes `height` rows of `width` gray pixels to `dst`; consecutive rows are `dstStride` bytes apart.
/// Preconditions: src.size() == width * height; every value in `src` is < lut.size(); dstStride >= width.
void applyGrayLut(std::span<const std::uint16_t> src, std::span<const std::uint8_t> lut, int width,
                  int height, std::uint8_t* dst, std::ptrdiff_t dstStride) noexcept;

} // namespace dv
