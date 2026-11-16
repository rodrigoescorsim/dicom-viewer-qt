#include "core/ImageDocument.hpp"
#include "core/LoadError.hpp"

#include <QTest>

#include <expected>
#include <string>
#include <utility>

using dv::ColorImage;
using dv::ImageDocument;
using dv::LoadError;
using dv::LoadErrorCode;
using dv::LoadResult;

class TstImageDocument : public QObject {
    Q_OBJECT

private slots:
    void defaultsAreSafe();
    void variantAccessors();
    void loadResult_carriesError();
    void loadErrorCode_hasEightValues();
};

void TstImageDocument::defaultsAreSafe() {
    const ImageDocument document{};
    QCOMPARE(document.width, 0);
    QCOMPARE(document.pixelAspectRatio, 1.0);
    QVERIFY(document.mono() != nullptr);
    QVERIFY(document.color() == nullptr);
    QCOMPARE(document.levelCount(), 0);
}

void TstImageDocument::variantAccessors() {
    ColorImage color;
    color.levels.resize(2);

    ImageDocument document;
    document.pixels = std::move(color);

    QVERIFY(document.color() != nullptr);
    QVERIFY(document.mono() == nullptr);
    QCOMPARE(document.levelCount(), 2);
}

void TstImageDocument::loadResult_carriesError() {
    const LoadResult<int> result = std::unexpected(LoadError{LoadErrorCode::NoPixelData, "x"});
    QVERIFY(!result);
    QCOMPARE(result.error().code, LoadErrorCode::NoPixelData);
    QCOMPARE(result.error().detail, std::string{"x"});
}

void TstImageDocument::loadErrorCode_hasEightValues() {
    // Guards ErrorMessages: a new code must come with a user-facing message.
    QCOMPARE(static_cast<int>(LoadErrorCode::OutOfMemory), 7);
}

QTEST_APPLESS_MAIN(TstImageDocument)

#include "tst_ImageDocument.moc"
