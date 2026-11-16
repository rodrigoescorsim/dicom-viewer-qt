#include "app/ImageViewport.hpp"

#include <QColor>
#include <QImage>
#include <QPointF>
#include <QTest>

#include <cstddef>
#include <cstdint>
#include <vector>

#include "support/TestDocuments.hpp"

using dv::VoiFunction;
using dv::VoiWindow;
using dv::app::ImageViewport;
using dv::test::makeColorDocument;
using dv::test::makeMonoDocument;

namespace {

/// Gray output == index for indices 0..255.
constexpr VoiWindow kIdentityWindow{.center = 128.0, .width = 256.0, .function = VoiFunction::Linear};
constexpr double kTolerance = 1e-6;
constexpr int kDarkLimit = 10;
constexpr int kBrightLimit = 245;

[[nodiscard]] bool near(double a, double b) {
    return qAbs(a - b) < kTolerance;
}

[[nodiscard]] bool near(QPointF a, QPointF b) {
    return near(a.x(), b.x()) && near(a.y(), b.y());
}

[[nodiscard]] std::vector<std::uint16_t> filled(int width, int height, std::uint16_t value) {
    return std::vector<std::uint16_t>(static_cast<std::size_t>(width) * static_cast<std::size_t>(height),
                                      value);
}

[[nodiscard]] int grayAt(const QImage& image, int x, int y) {
    return qGray(image.pixel(x, y));
}

/// Resizes, shows and waits until the viewport can be painted.
[[nodiscard]] bool showViewport(ImageViewport& viewport, int width, int height) {
    viewport.resize(width, height);
    viewport.show();
    return QTest::qWaitForWindowExposed(&viewport);
}

} // namespace

class TstImageViewport : public QObject {
    Q_OBJECT

private slots:
    void emptyState_paintsBlack();
    void rendersGrayValues();
    void rendersMonochrome1Inverted();
    void rendersOddWidthWithoutSkew();
    void rendersColor();
    void fitsToWindow_landscape();
    void fitsToWindow_portrait();
    void honoursPixelAspectRatio();
    void widgetToImage_isInverse();
    void refitsOnResize();
    void paintsImageNotMirrored();
    void paintsTopRowAtTop();
    void selectsPyramidLevel();
    void setDocument_resetsWindowAndEmits();
    void replacingColorDocument_isSafe();
};

void TstImageViewport::emptyState_paintsBlack() {
    ImageViewport viewport;
    QVERIFY(showViewport(viewport, 400, 300));
    QCOMPARE(viewport.grab().toImage().pixelColor(5, 5), QColor(Qt::black));
}

void TstImageViewport::rendersGrayValues() {
    ImageViewport viewport;
    QVERIFY(showViewport(viewport, 400, 300));
    viewport.setDocument(makeMonoDocument(2, 2, {0, 255, 128, 64}, kIdentityWindow));

    const QImage& frame = viewport.renderedFrame();
    QCOMPARE(frame.format(), QImage::Format_Grayscale8);
    QCOMPARE(frame.size(), QSize(2, 2));
    QCOMPARE(grayAt(frame, 0, 0), 0);
    QCOMPARE(grayAt(frame, 1, 0), 255);
    QCOMPARE(grayAt(frame, 0, 1), 128);
    QCOMPARE(grayAt(frame, 1, 1), 64);
}

void TstImageViewport::rendersMonochrome1Inverted() {
    ImageViewport viewport;
    QVERIFY(showViewport(viewport, 400, 300));
    viewport.setDocument(makeMonoDocument(2, 2, {0, 255, 128, 64}, kIdentityWindow, true));

    const QImage& frame = viewport.renderedFrame();
    QCOMPARE(grayAt(frame, 0, 0), 255);
    QCOMPARE(grayAt(frame, 1, 0), 0);
    QCOMPARE(grayAt(frame, 0, 1), 127);
    QCOMPARE(grayAt(frame, 1, 1), 191);
}

void TstImageViewport::rendersOddWidthWithoutSkew() {
    ImageViewport viewport;
    QVERIFY(showViewport(viewport, 400, 300));
    viewport.setDocument(makeMonoDocument(3, 2, {10, 20, 30, 40, 50, 60}, kIdentityWindow));

    const QImage& frame = viewport.renderedFrame();
    QCOMPARE(frame.size(), QSize(3, 2));
    int expected = 10;
    for (int y = 0; y < 2; ++y) {
        for (int x = 0; x < 3; ++x) {
            QCOMPARE(grayAt(frame, x, y), expected);
            expected += 10;
        }
    }
}

void TstImageViewport::rendersColor() {
    ImageViewport viewport;
    QVERIFY(showViewport(viewport, 400, 300));
    viewport.setDocument(makeColorDocument(3, 1, {255, 0, 0, 0, 255, 0, 0, 0, 255}));

    const QImage& frame = viewport.renderedFrame();
    QCOMPARE(frame.format(), QImage::Format_RGB888);
    QCOMPARE(frame.pixelColor(0, 0), QColor(Qt::red));
    QCOMPARE(frame.pixelColor(1, 0), QColor(Qt::green));
    QCOMPARE(frame.pixelColor(2, 0), QColor(Qt::blue));
}

void TstImageViewport::fitsToWindow_landscape() {
    ImageViewport viewport;
    QVERIFY(showViewport(viewport, 400, 300));
    viewport.setDocument(makeMonoDocument(200, 100, filled(200, 100, 0), kIdentityWindow));

    QVERIFY(near(viewport.zoom(), 2.0));
    QVERIFY(near(viewport.imageToWidget({0.0, 0.0}), {0.0, 50.0}));
    QVERIFY(near(viewport.imageToWidget({200.0, 100.0}), {400.0, 250.0}));
    QVERIFY(viewport.isFitToWindow());
}

void TstImageViewport::fitsToWindow_portrait() {
    ImageViewport viewport;
    QVERIFY(showViewport(viewport, 400, 300));
    viewport.setDocument(makeMonoDocument(100, 200, filled(100, 200, 0), kIdentityWindow));

    QVERIFY(near(viewport.zoom(), 1.5));
    QVERIFY(near(viewport.imageToWidget({0.0, 0.0}), {125.0, 0.0}));
}

void TstImageViewport::honoursPixelAspectRatio() {
    ImageViewport viewport;
    QVERIFY(showViewport(viewport, 400, 400));
    viewport.setDocument(makeMonoDocument(100, 100, filled(100, 100, 0), kIdentityWindow, false, 2.0));

    QVERIFY(near(viewport.zoom(), 2.0));
    QVERIFY(near(viewport.imageToWidget({0.0, 0.0}), {100.0, 0.0}));
    QVERIFY(near(viewport.imageToWidget({100.0, 100.0}), {300.0, 400.0}));
}

void TstImageViewport::widgetToImage_isInverse() {
    ImageViewport viewport;
    QVERIFY(showViewport(viewport, 400, 300));
    viewport.setDocument(makeMonoDocument(120, 80, filled(120, 80, 0), kIdentityWindow, false, 1.5));

    for (const QPointF point : {QPointF(0.0, 0.0), QPointF(37.25, 12.5), QPointF(120.0, 80.0)}) {
        const QPointF roundTrip = viewport.widgetToImage(viewport.imageToWidget(point));
        QVERIFY2(near(roundTrip, point),
                 qPrintable(QStringLiteral("(%1, %2)").arg(point.x()).arg(point.y())));
    }
}

void TstImageViewport::refitsOnResize() {
    ImageViewport viewport;
    QVERIFY(showViewport(viewport, 400, 300));
    viewport.setDocument(makeMonoDocument(200, 100, filled(200, 100, 0), kIdentityWindow));

    viewport.resize(800, 300);
    QVERIFY(near(viewport.zoom(), 3.0));
}

void TstImageViewport::paintsImageNotMirrored() {
    std::vector<std::uint16_t> indices(16);
    for (std::size_t i = 0; i < indices.size(); ++i) {
        indices[i] = (i % 4) < 2 ? std::uint16_t{0} : std::uint16_t{255}; // columns 0-1 dark, 2-3 bright
    }
    ImageViewport viewport;
    QVERIFY(showViewport(viewport, 400, 400));
    viewport.setDocument(makeMonoDocument(4, 4, std::move(indices), kIdentityWindow));

    const QImage screen = viewport.grab().toImage();
    QVERIFY(qGray(screen.pixel(50, 200)) < kDarkLimit);
    QVERIFY(qGray(screen.pixel(350, 200)) > kBrightLimit);
}

void TstImageViewport::paintsTopRowAtTop() {
    std::vector<std::uint16_t> indices(16);
    for (std::size_t i = 0; i < indices.size(); ++i) {
        indices[i] = (i / 4) < 2 ? std::uint16_t{0} : std::uint16_t{255}; // rows 0-1 dark, 2-3 bright
    }
    ImageViewport viewport;
    QVERIFY(showViewport(viewport, 400, 400));
    viewport.setDocument(makeMonoDocument(4, 4, std::move(indices), kIdentityWindow));

    const QImage screen = viewport.grab().toImage();
    QVERIFY(qGray(screen.pixel(200, 50)) < kDarkLimit);
    QVERIFY(qGray(screen.pixel(200, 350)) > kBrightLimit);
}

void TstImageViewport::selectsPyramidLevel() {
    constexpr int kSide = 1024;
    ImageViewport viewport;
    QVERIFY(showViewport(viewport, 256, 256));
    viewport.setDocument(makeMonoDocument(kSide, kSide, filled(kSide, kSide, 7), kIdentityWindow));
    QCOMPARE(viewport.renderedFrame().width(), 256);

    viewport.resize(kSide, kSide);
    QCOMPARE(viewport.renderedFrame().width(), kSide);
}

void TstImageViewport::setDocument_resetsWindowAndEmits() {
    constexpr VoiWindow kOtherWindow{.center = 1000.0, .width = 500.0, .function = VoiFunction::Sigmoid};
    ImageViewport viewport;
    QVERIFY(showViewport(viewport, 400, 300));
    viewport.setDocument(makeMonoDocument(2, 2, {0, 1, 2, 3}, kIdentityWindow));

    int windowSignals = 0;
    int zoomSignals = 0;
    VoiWindow emittedWindow;
    connect(&viewport, &ImageViewport::windowChanged, this, [&](const VoiWindow& window) {
        ++windowSignals;
        emittedWindow = window;
    });
    connect(&viewport, &ImageViewport::zoomChanged, this, [&](double) { ++zoomSignals; });

    viewport.setDocument(makeMonoDocument(2, 2, {0, 1, 2, 3}, kOtherWindow));
    QCOMPARE(viewport.window(), kOtherWindow);
    QVERIFY(windowSignals >= 1);
    QCOMPARE(emittedWindow, kOtherWindow);
    QVERIFY(zoomSignals >= 1);
}

void TstImageViewport::replacingColorDocument_isSafe() {
    ImageViewport viewport;
    QVERIFY(showViewport(viewport, 400, 300));

    viewport.setDocument(makeColorDocument(3, 1, {255, 0, 0, 0, 255, 0, 0, 0, 255}));
    QVERIFY(!viewport.grab().isNull());
    viewport.setDocument(makeColorDocument(2, 2, std::vector<std::uint8_t>(12, 200)));
    QVERIFY(!viewport.grab().isNull());
    viewport.setDocument(nullptr);
    QVERIFY(!viewport.grab().isNull());
    QVERIFY(viewport.renderedFrame().isNull());
}

QTEST_MAIN(TstImageViewport)
#include "tst_ImageViewport.moc"
