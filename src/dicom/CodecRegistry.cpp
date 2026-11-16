#include "dicom/CodecRegistry.hpp"

#include <dcmtk/config/osconfig.h>
#include <dcmtk/dcmdata/dcrledrg.h>
#include <dcmtk/dcmjpeg/djdecode.h>
#include <dcmtk/dcmjpls/djdecode.h>
#include <dcmtk/oflog/oflog.h>

namespace dv {

CodecRegistry::CodecRegistry() {
    // DCMTK logs parse warnings to stderr by default; failures are reported through LoadError instead.
    OFLog::configure(OFLogger::OFF_LOG_LEVEL);
    DJDecoderRegistration::registerCodecs();
    DJLSDecoderRegistration::registerCodecs();
    DcmRLEDecoderRegistration::registerCodecs();
}

CodecRegistry::~CodecRegistry() {
    DcmRLEDecoderRegistration::cleanup();
    DJLSDecoderRegistration::cleanup();
    DJDecoderRegistration::cleanup();
}

} // namespace dv
