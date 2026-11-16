#pragma once

namespace dv {

/// RAII guard for the process-wide DCMTK decompression codecs (JPEG, JPEG-LS, RLE).
/// Create exactly one instance at the top of main() and keep it alive until every load has finished.
class CodecRegistry {
public:
    CodecRegistry();
    ~CodecRegistry();

    CodecRegistry(const CodecRegistry&) = delete;
    CodecRegistry& operator=(const CodecRegistry&) = delete;
    CodecRegistry(CodecRegistry&&) = delete;
    CodecRegistry& operator=(CodecRegistry&&) = delete;
};

} // namespace dv
