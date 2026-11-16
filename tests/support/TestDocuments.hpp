#pragma once

#include "app/DocumentLoader.hpp" // DocumentPtr
#include "core/ImageDocument.hpp"
#include "core/Raster.hpp"
#include "core/Voi.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

namespace dv::test {

/// Monochrome document whose pixel indices are `indices` (row-major), identity mapping
/// (value == index), lutEntryCount = maxIndex + 1, defaultWindow = `window`.
[[nodiscard]] inline dv::app::DocumentPtr makeMonoDocument(int width, int height,
                                                           std::vector<std::uint16_t> indices,
                                                           dv::VoiWindow window, bool inverted = false,
                                                           double pixelAspectRatio = 1.0) {
    assert(indices.size() == static_cast<std::size_t>(width) * static_cast<std::size_t>(height));
    const auto [minIt, maxIt] = std::ranges::minmax_element(indices);

    dv::MonoImage mono;
    mono.lutEntryCount = std::uint32_t{*maxIt} + 1U;
    mono.inverted = inverted;
    mono.minValue = *minIt;
    mono.maxValue = *maxIt;
    mono.defaultWindow = window;
    mono.levels = dv::buildPyramid(
        dv::GrayRaster{.width = width, .height = height, .channels = 1, .data = std::move(indices)});

    dv::ImageDocument document;
    document.width = width;
    document.height = height;
    document.pixelAspectRatio = pixelAspectRatio;
    document.pixels = std::move(mono);
    return std::make_shared<const dv::ImageDocument>(std::move(document));
}

/// Color document from interleaved RGB bytes.
[[nodiscard]] inline dv::app::DocumentPtr makeColorDocument(int width, int height,
                                                            std::vector<std::uint8_t> rgb) {
    assert(rgb.size() == static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 3U);

    dv::ColorImage color;
    color.levels = dv::buildPyramid(
        dv::RgbRaster{.width = width, .height = height, .channels = 3, .data = std::move(rgb)});

    dv::ImageDocument document;
    document.width = width;
    document.height = height;
    document.pixels = std::move(color);
    return std::make_shared<const dv::ImageDocument>(std::move(document));
}

} // namespace dv::test
