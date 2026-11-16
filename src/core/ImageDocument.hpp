#pragma once

#include "core/GrayLut.hpp"
#include "core/Orientation.hpp"
#include "core/Raster.hpp"
#include "core/Voi.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace dv {

/// One node of the metadata tree, already converted to display strings (UTF-8).
struct MetadataNode {
    std::uint16_t group{0};
    std::uint16_t element{0};
    std::string name;  ///< Dictionary name, e.g. "PatientName". Empty for item nodes.
    std::string vr;    ///< "PN", "SQ", ... Empty for item nodes.
    std::string value; ///< Text value (multiple values joined by '\'). Empty for binary, sequences, items.
    std::vector<MetadataNode> children; ///< Sequence -> item nodes. Item node -> its elements.
    bool isItem{false};   ///< true for the node that represents one item of a sequence.
    bool isBinary{false}; ///< true for binary VRs (OB, OW, UN...) and Pixel Data; see byteLength.
    std::uint64_t byteLength{0}; ///< Size of the binary value. 0 when unknown (compressed Pixel Data).
};

/// Grayscale image after the Modality LUT. Pixels are 16-bit indices (see ValueMapping).
struct MonoImage {
    std::vector<GrayRaster> levels; ///< Resolution pyramid; levels[0] is the full image.
    ValueMapping mapping;
    std::uint32_t lutEntryCount{1}; ///< Highest index in levels[0] + 1.
    bool inverted{false};           ///< MONOCHROME1.
    std::optional<IndexRange> padding;
    double minValue{0.0}; ///< Smallest modality value, ignoring padding pixels.
    double maxValue{0.0}; ///< Largest modality value, ignoring padding pixels.
    std::vector<WindowPreset> presets; ///< Valid Window Center/Width pairs of the file, in file order.
    VoiWindow defaultWindow; ///< presets[0].window, or the full range when there are no presets.
};

/// Color image already converted to 8-bit RGB.
struct ColorImage {
    std::vector<RgbRaster> levels; ///< Resolution pyramid; levels[0] is the full image.
};

struct ImageInfo {
    std::string transferSyntaxName;
    std::string photometricInterpretation; ///< As written in the file, e.g. "YBR_FULL_422".
    std::string modality;
    int bitsAllocated{0};
    int bitsStored{0};
    int frameCount{1}; ///< Frames in the file. Only the first one is loaded.
};

struct ImageDocument {
    std::filesystem::path sourcePath;
    int width{0};  ///< Columns.
    int height{0}; ///< Rows.
    double pixelAspectRatio{1.0}; ///< Pixel height / pixel width. 1.0 = square pixels.
    std::variant<MonoImage, ColorImage> pixels;
    EdgeLabels orientation;
    ImageInfo info;
    std::vector<MetadataNode> metadata; ///< File Meta Information elements followed by the dataset's.

    [[nodiscard]] const MonoImage* mono() const noexcept { return std::get_if<MonoImage>(&pixels); }
    [[nodiscard]] const ColorImage* color() const noexcept { return std::get_if<ColorImage>(&pixels); }
    [[nodiscard]] int levelCount() const noexcept {
        return std::visit([](const auto& image) { return static_cast<int>(image.levels.size()); }, pixels);
    }
};

} // namespace dv
