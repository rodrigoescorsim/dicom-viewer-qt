#pragma once

#include <cstdint>
#include <expected>
#include <string>

namespace dv {

enum class LoadErrorCode : std::uint8_t {
    FileNotFound,              ///< The path does not exist.
    FileNotReadable,           ///< Not a regular file, or it cannot be opened for reading.
    NotDicom,                  ///< Empty file, or content that is not a DICOM dataset.
    NoPixelData,               ///< Valid DICOM without Pixel Data (SR, DICOMDIR, RT Plan...).
    UnsupportedTransferSyntax, ///< Compressed with a codec that is not available. detail = syntax name.
    UnsupportedImage,          ///< Image attributes are missing, invalid or not supported.
    CorruptPixelData,          ///< DICOM file that is truncated, or whose Pixel Data cannot be decoded.
    OutOfMemory,
};

struct LoadError {
    LoadErrorCode code{LoadErrorCode::NotDicom};
    std::string detail; ///< Technical detail in English (library message). May be empty.
};

template <typename T>
using LoadResult = std::expected<T, LoadError>;

} // namespace dv
