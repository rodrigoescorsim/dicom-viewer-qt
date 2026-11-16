#pragma once

#include "core/ImageDocument.hpp"

#include <array>
#include <cassert>
#include <cstddef>
#include <filesystem>
#include <string_view>

namespace dv::test {

/// Absolute path of a file in samples/ (DV_SAMPLES_DIR is defined by dv_add_test).
[[nodiscard]] inline std::filesystem::path samplePath(std::string_view fileName) {
    return std::filesystem::path(DV_SAMPLES_DIR) / fileName;
}

/// Modality value of pixel (column, row) of level 0 of a monochrome document.
[[nodiscard]] inline double monoValueAt(const dv::ImageDocument& document, int column, int row) {
    const dv::MonoImage* mono = document.mono();
    assert(mono != nullptr && !mono->levels.empty());
    const dv::GrayRaster& level0 = mono->levels.front();
    const std::size_t index = static_cast<std::size_t>(row) * static_cast<std::size_t>(level0.width) +
                              static_cast<std::size_t>(column);
    return mono->mapping.valueOf(level0.data[index]);
}

/// RGB triple of pixel (column, row) of level 0 of a color document.
[[nodiscard]] inline std::array<int, 3> rgbAt(const dv::ImageDocument& document, int column, int row) {
    const dv::ColorImage* color = document.color();
    assert(color != nullptr && !color->levels.empty());
    const dv::RgbRaster& level0 = color->levels.front();
    const std::size_t pixel = static_cast<std::size_t>(row) * static_cast<std::size_t>(level0.width) +
                              static_cast<std::size_t>(column);
    const std::size_t offset = pixel * static_cast<std::size_t>(level0.channels);
    return {level0.data[offset], level0.data[offset + 1], level0.data[offset + 2]};
}

} // namespace dv::test
