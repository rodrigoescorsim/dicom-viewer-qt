#include "dicom/DicomLoader.hpp"

#include <dcmtk/config/osconfig.h>
#include <dcmtk/dcmdata/dccodec.h>
#include <dcmtk/dcmdata/dctk.h>
#include <dcmtk/dcmdata/dcxfer.h>
#include <dcmtk/dcmimage/diregist.h> // registers color image support in DicomImage
#include <dcmtk/dcmimgle/dcmimage.h>
#include <dcmtk/dcmimgle/dipixel.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <fstream>
#include <new>
#include <span>
#include <system_error>
#include <utility>

namespace dv {

namespace {

constexpr double kMaxIndex = 65535.0;

std::unexpected<LoadError> fail(LoadErrorCode code, std::string detail = {}) {
    return std::unexpected(LoadError{.code = code, .detail = std::move(detail)});
}

// ---------------------------------------------------------------------------------------------------------
// Dataset helpers
// ---------------------------------------------------------------------------------------------------------

std::string stringOf(DcmItem& item, const DcmTagKey& key, unsigned long position = 0) {
    OFString value;
    if (item.findAndGetOFString(key, value, position).good()) {
        return {value.c_str(), value.size()};
    }
    return {};
}

int intOf(DcmItem& item, const DcmTagKey& key, int fallback) {
    Uint16 value = 0;
    return item.findAndGetUint16(key, value).good() ? static_cast<int>(value) : fallback;
}

double doubleOf(DcmItem& item, const DcmTagKey& key, double fallback) {
    Float64 value = 0.0;
    return item.findAndGetFloat64(key, value).good() && std::isfinite(value) ? value : fallback;
}

// ---------------------------------------------------------------------------------------------------------
// Monochrome pixels
// ---------------------------------------------------------------------------------------------------------

/// Converts the modality values produced by DCMTK into 16-bit indices and fills mapping/lutEntryCount.
template <typename T>
GrayRaster toIndices(std::span<const T> samples, int width, int height, MonoImage& image) {
    const auto [minIt, maxIt] = std::ranges::minmax_element(samples);
    const double minValue = static_cast<double>(*minIt);
    const double range = static_cast<double>(*maxIt) - minValue;

    GrayRaster raster{.width = width, .height = height, .channels = 1, .data = {}};
    raster.data.resize(samples.size());

    if (range <= kMaxIndex) {
        // Common case: one index per modality value, no loss.
        const auto base = static_cast<std::int64_t>(*minIt);
        std::ranges::transform(samples, raster.data.begin(), [base](T sample) {
            return static_cast<std::uint16_t>(static_cast<std::int64_t>(sample) - base);
        });
        image.mapping = {.offset = minValue, .scale = 1.0};
        image.lutEntryCount = static_cast<std::uint32_t>(range) + 1U;
    } else {
        // Dynamic range above 16 bits (e.g. 32-bit dose grids): quantize to 65536 levels.
        const double scale = range / kMaxIndex;
        std::ranges::transform(samples, raster.data.begin(), [minValue, scale](T sample) {
            return static_cast<std::uint16_t>(std::lround((static_cast<double>(sample) - minValue) / scale));
        });
        image.mapping = {.offset = minValue, .scale = scale};
        image.lutEntryCount = 65536U;
    }
    return raster;
}

/// Pixel Padding Value (0028,0120) / Pixel Padding Range Limit (0028,0121) expressed as indices.
/// The attributes are defined on stored values, so they are only honoured when the modality
/// transformation is a rescale with positive slope (or absent).
std::optional<IndexRange> readPadding(DcmDataset& dataset, const MonoImage& image) {
    long paddingValue = 0;
    if (dataset.findAndGetLongInt(DCM_PixelPaddingValue, paddingValue).bad() ||
        dataset.tagExists(DCM_ModalityLUTSequence)) {
        return std::nullopt;
    }
    long rangeLimit = paddingValue;
    if (dataset.findAndGetLongInt(DCM_PixelPaddingRangeLimit, rangeLimit).bad()) {
        rangeLimit = paddingValue;
    }
    const double slope = doubleOf(dataset, DCM_RescaleSlope, 1.0);
    const double intercept = doubleOf(dataset, DCM_RescaleIntercept, 0.0);
    if (!(slope > 0.0)) {
        return std::nullopt;
    }

    const double low = static_cast<double>(std::min(paddingValue, rangeLimit)) * slope + intercept;
    const double high = static_cast<double>(std::max(paddingValue, rangeLimit)) * slope + intercept;
    const double lastIndex = static_cast<double>(image.lutEntryCount - 1U);
    const double first = std::ceil((low - image.mapping.offset) / image.mapping.scale - 1e-9);
    const double last = std::floor((high - image.mapping.offset) / image.mapping.scale + 1e-9);
    if (last < 0.0 || first > lastIndex || last < first) {
        return std::nullopt; // no pixel of this image falls in the padding range
    }
    return IndexRange{.first = static_cast<std::uint16_t>(std::max(first, 0.0)),
                      .last = static_cast<std::uint16_t>(std::min(last, lastIndex))};
}

/// Smallest and largest modality value among the pixels that are not padding.
void computeValueRange(MonoImage& image) {
    const std::vector<std::uint16_t>& indices = image.levels.front().data;
    std::uint32_t minIndex = 65536U;
    std::uint32_t maxIndex = 0U;
    bool found = false;
    for (const std::uint16_t index : indices) {
        if (image.padding && index >= image.padding->first && index <= image.padding->last) {
            continue;
        }
        minIndex = std::min<std::uint32_t>(minIndex, index);
        maxIndex = std::max<std::uint32_t>(maxIndex, index);
        found = true;
    }
    if (!found) { // every pixel is padding: fall back to the whole range
        minIndex = 0U;
        maxIndex = image.lutEntryCount - 1U;
    }
    image.minValue = image.mapping.valueOf(minIndex);
    image.maxValue = image.mapping.valueOf(maxIndex);
}

VoiFunction readVoiFunction(DcmDataset& dataset) {
    const std::string function = stringOf(dataset, DCM_VOILUTFunction);
    if (function == "SIGMOID") {
        return VoiFunction::Sigmoid;
    }
    if (function == "LINEAR_EXACT") {
        return VoiFunction::LinearExact;
    }
    return VoiFunction::Linear;
}

/// Window Center/Width are multi-valued and paired by position. Invalid pairs are skipped.
std::vector<WindowPreset> readPresets(DcmDataset& dataset) {
    const VoiFunction function = readVoiFunction(dataset);
    std::vector<WindowPreset> presets;
    for (unsigned long i = 0;; ++i) {
        Float64 center = 0.0;
        Float64 width = 0.0;
        if (dataset.findAndGetFloat64(DCM_WindowCenter, center, i).bad() ||
            dataset.findAndGetFloat64(DCM_WindowWidth, width, i).bad()) {
            break;
        }
        if (!std::isfinite(center) || !std::isfinite(width) || width < kMinWindowWidth) {
            continue;
        }
        presets.push_back({.label = stringOf(dataset, DCM_WindowCenterWidthExplanation, i),
                           .window = {.center = center, .width = width, .function = function}});
    }
    return presets;
}

LoadResult<MonoImage> extractMono(DicomImage& source, DcmDataset& dataset) {
    const DiPixel* inter = source.getInterData();
    const int width = static_cast<int>(source.getWidth());
    const int height = static_cast<int>(source.getHeight());
    const std::size_t count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    if (inter == nullptr || inter->getData() == nullptr || inter->getCount() < count) {
        return fail(LoadErrorCode::CorruptPixelData, "pixel data is missing or shorter than Rows x Columns");
    }

    MonoImage image;
    GrayRaster level0;
    const void* data = inter->getData();
    switch (inter->getRepresentation()) {
    case EPR_Uint8:
        level0 = toIndices(std::span(static_cast<const Uint8*>(data), count), width, height, image);
        break;
    case EPR_Sint8:
        level0 = toIndices(std::span(static_cast<const Sint8*>(data), count), width, height, image);
        break;
    case EPR_Uint16:
        level0 = toIndices(std::span(static_cast<const Uint16*>(data), count), width, height, image);
        break;
    case EPR_Sint16:
        level0 = toIndices(std::span(static_cast<const Sint16*>(data), count), width, height, image);
        break;
    case EPR_Uint32:
        level0 = toIndices(std::span(static_cast<const Uint32*>(data), count), width, height, image);
        break;
    case EPR_Sint32:
        level0 = toIndices(std::span(static_cast<const Sint32*>(data), count), width, height, image);
        break;
    default:
        return fail(LoadErrorCode::UnsupportedImage, "unsupported pixel representation");
    }

    image.levels = buildPyramid(std::move(level0));
    image.inverted = source.getPhotometricInterpretation() == EPI_Monochrome1;
    image.padding = readPadding(dataset, image);
    computeValueRange(image);
    image.presets = readPresets(dataset);
    image.defaultWindow = image.presets.empty() ? fullRangeWindow(image.minValue, image.maxValue)
                                                : image.presets.front().window;
    return image;
}

// ---------------------------------------------------------------------------------------------------------
// Color pixels
// ---------------------------------------------------------------------------------------------------------

LoadResult<ColorImage> extractColor(DicomImage& source) {
    const int width = static_cast<int>(source.getWidth());
    const int height = static_cast<int>(source.getHeight());
    const std::size_t expected = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 3U;

    // 8 bits per sample, frame 0, planar = 0 -> interleaved R,G,B whatever the source color model is.
    const unsigned long size = source.getOutputDataSize(8);
    if (size < expected) {
        return fail(LoadErrorCode::CorruptPixelData, "color pixel data is shorter than Rows x Columns x 3");
    }
    RgbRaster level0{.width = width, .height = height, .channels = 3, .data = {}};
    level0.data.resize(size);
    if (source.getOutputData(level0.data.data(), size, 8, 0, 0) == 0) {
        return fail(LoadErrorCode::CorruptPixelData, "color pixel data could not be rendered");
    }
    level0.data.resize(expected);

    ColorImage image;
    image.levels = buildPyramid(std::move(level0));
    return image;
}

// ---------------------------------------------------------------------------------------------------------
// Presentation attributes
// ---------------------------------------------------------------------------------------------------------

EdgeLabels readOrientation(DcmDataset& dataset) {
    const std::string rowDirection = stringOf(dataset, DCM_PatientOrientation, 0);
    const std::string columnDirection = stringOf(dataset, DCM_PatientOrientation, 1);
    if (!rowDirection.empty() && !columnDirection.empty()) {
        return edgeLabelsFromPatientOrientation(rowDirection, columnDirection);
    }
    std::array<double, 6> cosines{};
    for (unsigned long i = 0; i < cosines.size(); ++i) {
        if (dataset.findAndGetFloat64(DCM_ImageOrientationPatient, cosines[i], i).bad()) {
            return {};
        }
    }
    return edgeLabelsFromImageOrientation(cosines);
}

double readPixelAspectRatio(DicomImage& source) {
    // Pixel height / pixel width, from Pixel Spacing, Imager Pixel Spacing or Pixel Aspect Ratio.
    const double ratio = source.getHeightWidthRatio();
    if (!std::isfinite(ratio) || ratio <= 0.0 || std::abs(ratio - 1.0) < 1e-3) {
        return 1.0;
    }
    return ratio;
}

ImageInfo readInfo(DcmDataset& dataset, const DcmXfer& transferSyntax) {
    Sint32 frames = 1;
    if (dataset.findAndGetSint32(DCM_NumberOfFrames, frames).bad() || frames < 1) {
        frames = 1;
    }
    return {.transferSyntaxName = transferSyntax.getXferName(),
            .photometricInterpretation = stringOf(dataset, DCM_PhotometricInterpretation),
            .modality = stringOf(dataset, DCM_Modality),
            .bitsAllocated = intOf(dataset, DCM_BitsAllocated, 0),
            .bitsStored = intOf(dataset, DCM_BitsStored, 0),
            .frameCount = static_cast<int>(frames)};
}

// ---------------------------------------------------------------------------------------------------------
// Metadata
// ---------------------------------------------------------------------------------------------------------

bool isBinary(DcmElement& element) {
    if (element.getTag() == DCM_PixelData) {
        return true;
    }
    switch (element.getVR()) {
    case EVR_OB:
    case EVR_OD:
    case EVR_OF:
    case EVR_OL:
    case EVR_OV:
    case EVR_OW:
    case EVR_UN:
    case EVR_ox:
    case EVR_px:
        return true;
    default:
        return false;
    }
}

void appendElements(DcmItem& item, std::vector<MetadataNode>& out);

MetadataNode toNode(DcmElement& element) {
    DcmTag tag = element.getTag();
    MetadataNode node;
    node.group = tag.getGroup();
    node.element = tag.getElement();
    node.name = tag.getTagName();
    node.vr = DcmVR(element.getVR()).getVRName();

    if (auto* sequence = dynamic_cast<DcmSequenceOfItems*>(&element)) {
        node.children.reserve(sequence->card());
        for (unsigned long i = 0; i < sequence->card(); ++i) {
            MetadataNode itemNode;
            itemNode.group = node.group;
            itemNode.element = node.element;
            itemNode.isItem = true;
            if (DcmItem* item = sequence->getItem(i)) {
                appendElements(*item, itemNode.children);
            }
            node.children.push_back(std::move(itemNode));
        }
    } else if (isBinary(element)) {
        node.isBinary = true;
        node.byteLength = element.getLength();
    } else {
        OFString value;
        if (element.getOFStringArray(value).good()) {
            node.value.assign(value.c_str(), value.size());
        }
    }
    return node;
}

void appendElements(DcmItem& item, std::vector<MetadataNode>& out) {
    out.reserve(out.size() + item.card());
    for (unsigned long i = 0; i < item.card(); ++i) {
        if (DcmElement* element = item.getElement(i)) {
            out.push_back(toNode(*element));
        }
    }
}

// ---------------------------------------------------------------------------------------------------------
// Loading
// ---------------------------------------------------------------------------------------------------------

LoadErrorCode codeForImageStatus(EI_Status status) {
    switch (status) {
    case EIS_MemoryFailure:
        return LoadErrorCode::OutOfMemory;
    case EIS_InvalidImage:
    case EIS_InvalidDocument:
        return LoadErrorCode::CorruptPixelData;
    default:
        return LoadErrorCode::UnsupportedImage;
    }
}

LoadResult<ImageDocument> load(const std::filesystem::path& path) {
    // 1. File system checks, so that I/O problems are not reported as "invalid DICOM".
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        return fail(LoadErrorCode::FileNotFound);
    }
    if (!std::filesystem::is_regular_file(path, ec)) {
        return fail(LoadErrorCode::FileNotReadable, "not a regular file");
    }
    bool hasDicomPrefix = false;
    {
        std::ifstream stream(path, std::ios::binary);
        if (!stream) {
            return fail(LoadErrorCode::FileNotReadable, "the file cannot be opened for reading");
        }
        std::array<char, 132> header{};
        stream.read(header.data(), static_cast<std::streamsize>(header.size()));
        const std::streamsize read = stream.gcount();
        if (read == 0) {
            return fail(LoadErrorCode::NotDicom, "the file is empty");
        }
        hasDicomPrefix = read == static_cast<std::streamsize>(header.size()) &&
                         std::memcmp(header.data() + 128, "DICM", 4) == 0;
    }

    // 2. Parse. path.c_str() is wchar_t* on Windows, so non-ASCII paths work.
    DcmFileFormat file;
    if (const OFCondition status = file.loadFile(OFFilename(path.c_str())); status.bad()) {
        // With the "DICM" prefix the file is DICOM but damaged; without it, it is not DICOM.
        return fail(hasDicomPrefix ? LoadErrorCode::CorruptPixelData : LoadErrorCode::NotDicom,
                    status.text());
    }
    DcmDataset& dataset = *file.getDataset();
    // DCMTK accepts legacy files without preamble. Without the "DICM" prefix, require an attribute that
    // identifies a DICOM object, so arbitrary files are rejected.
    if (!hasDicomPrefix && !dataset.tagExists(DCM_SOPClassUID) &&
        !(dataset.tagExists(DCM_Rows) && dataset.tagExists(DCM_Columns))) {
        return fail(LoadErrorCode::NotDicom, "no DICM prefix and no recognizable DICOM attributes");
    }
    if (!dataset.tagExists(DCM_PixelData)) {
        return fail(LoadErrorCode::NoPixelData);
    }

    // 3. Make sure a decoder is registered for the transfer syntax before touching the pixels.
    const E_TransferSyntax transferSyntax = dataset.getOriginalXfer();
    const DcmXfer transferSyntaxInfo(transferSyntax);
    if (transferSyntaxInfo.usesEncapsulatedFormat() && transferSyntaxInfo.isPixelDataCompressed() &&
        !DcmCodecList::canChangeCoding(transferSyntax, EXS_LittleEndianExplicit)) {
        return fail(LoadErrorCode::UnsupportedTransferSyntax, transferSyntaxInfo.getXferName());
    }

    // 4. Decode the first frame and apply the Modality LUT.
    DicomImage source(&file, transferSyntax, CIF_UsePartialAccessToPixelData, 0 /*first frame*/, 1 /*count*/);
    if (source.getStatus() != EIS_Normal) {
        return fail(codeForImageStatus(source.getStatus()), DicomImage::getString(source.getStatus()));
    }
    if (source.getWidth() == 0 || source.getHeight() == 0) {
        return fail(LoadErrorCode::UnsupportedImage, "image has zero rows or columns");
    }

    ImageDocument document;
    document.sourcePath = path;
    document.width = static_cast<int>(source.getWidth());
    document.height = static_cast<int>(source.getHeight());
    document.pixelAspectRatio = readPixelAspectRatio(source);

    if (source.isMonochrome()) {
        auto mono = extractMono(source, dataset);
        if (!mono) {
            return std::unexpected(std::move(mono.error()));
        }
        document.pixels = std::move(*mono);
    } else {
        auto color = extractColor(source);
        if (!color) {
            return std::unexpected(std::move(color.error()));
        }
        document.pixels = std::move(*color);
    }

    // 5. Text attributes, as UTF-8. If the conversion fails (unknown character set), values stay as stored.
    (void)dataset.convertToUTF8();
    document.orientation = readOrientation(dataset);
    document.info = readInfo(dataset, transferSyntaxInfo);
    appendElements(*file.getMetaInfo(), document.metadata);
    appendElements(dataset, document.metadata);
    return document;
}

} // namespace

std::string_view dicomToolkitVersion() noexcept {
    return OFFIS_DCMTK_VERSION_STRING;
}

LoadResult<ImageDocument> loadDicomFile(const std::filesystem::path& path) {
    try {
        return load(path);
    } catch (const std::bad_alloc&) {
        return fail(LoadErrorCode::OutOfMemory);
    } catch (const std::exception& error) {
        return fail(LoadErrorCode::UnsupportedImage, error.what());
    }
}

} // namespace dv
