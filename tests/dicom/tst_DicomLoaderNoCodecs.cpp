#include "dicom/DicomLoader.hpp"

#include <QTest>

#include "support/TestSupport.hpp"

// Separate executable on purpose: the codecs are registered process-wide, so this test must run in a
// process where no dv::CodecRegistry was ever created.
class TstDicomLoaderNoCodecs : public QObject {
    Q_OBJECT

private slots:
    void withoutCodecs();
};

void TstDicomLoaderNoCodecs::withoutCodecs() {
    const auto result = dv::loadDicomFile(dv::test::samplePath("anonymized_mamo.dcm"));
    QVERIFY(!result.has_value());
    QCOMPARE(result.error().code, dv::LoadErrorCode::UnsupportedTransferSyntax);
    QVERIFY2(result.error().detail.contains("JPEG Lossless"), result.error().detail.c_str());
}

QTEST_APPLESS_MAIN(TstDicomLoaderNoCodecs)

#include "tst_DicomLoaderNoCodecs.moc"
