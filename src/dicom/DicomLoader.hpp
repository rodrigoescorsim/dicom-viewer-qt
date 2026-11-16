#pragma once

#include "core/ImageDocument.hpp"
#include "core/LoadError.hpp"

#include <filesystem>
#include <string_view>

namespace dv {

/// Reads a DICOM file and returns everything the viewer needs to display it.
///
/// Thread-safe as long as a CodecRegistry is alive. Never throws: every failure, including
/// std::bad_alloc, is reported as a LoadError. Only the first frame of multi-frame files is loaded.
[[nodiscard]] LoadResult<ImageDocument> loadDicomFile(const std::filesystem::path& path);

/// Version of the DCMTK toolkit this library was built with, e.g. "3.7.1".
[[nodiscard]] std::string_view dicomToolkitVersion() noexcept;

} // namespace dv
