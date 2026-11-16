#include "app/ImageViewport.hpp"

#include <QApplication>
#include <QColor>
#include <QImage>
#include <QPointF>
#include <QTest>
#include <QWheelEvent>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "support/TestDocuments.hpp"

using dv::VoiFunction;
using dv::VoiWindow;
using dv::app::ImageViewport;
using dv::app::PixelProbe;
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

constexpr int kWheelStep = 120;
constexpr double kWheelZoomBase = 1.2;
constexpr double kButtonZoomStep = 1.25;

/// Sends a vertical wheel event of `delta` units at `position`.
void sendWheel(ImageViewport& viewport, QPointF position, int delta) {
    QWheelEvent event(position, viewport.mapToGlobal(position), QPoint(), QPoint(0, delta), Qt::NoButton,
                      Qt::NoModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(&viewport, &event);
}

/// Mono 200x100 document; fitted into a 400x300 widget its zoom is 2.0.
[[nodiscard]] dv::app::DocumentPtr landscapeDocument() {
    return makeMonoDocument(200, 100, filled(200, 100, 0), kIdentityWindow);
}

constexpr VoiWindow kDragWindow{.center = 1000.0, .width = 500.0, .function = VoiFunction::Linear};

/// Mono 64x64 with indices 0..2048: a window drag moves 2 modality units per mouse pixel.
[[nodiscard]] dv::app::DocumentPtr windowDragDocument(VoiWindow window = kDragWindow) {
    constexpr std::size_t kSide = 64;
    constexpr std::size_t kMaxIndex = 2048;
    std::vector<std::uint16_t> indices(kSide * kSide);
    for (std::size_t i = 0; i < indices.size(); ++i) {
        indices[i] = static_cast<std::uint16_t>(i % (kMaxIndex + 1));
    }
    return makeMonoDocument(static_cast<int>(kSide), static_cast<int>(kSide), std::move(indices), window);
}

/// Left-button drag from `from` to `to`, with `modifiers`.
void leftDrag(ImageViewport& viewport, QPoint from, QPoint to,
              Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
    QTest::mousePress(&viewport, Qt::LeftButton, modifiers, from);
    QTest::mouseMove(&viewport, to);
    QTest::mouseRelease(&viewport, Qt::LeftButton, modifiers, to);
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

    // T10: zoom, pan and pixel probe
    void wheelZoom_keepsPointUnderCursor();
    void wheelZoom_out();
    void zoom_isClamped();
    void zoomInOut_buttons();
    void actualSize();
    void doubleClick_fits();
    void middleDrag_pans();
    void ctrlLeftDrag_pans();
    void pan_isClamped();
    void resizeAfterZoom_keepsZoom();
    void probe_reportsValue();
    void probe_reportsColor();
    void probe_outsideImage_isEmpty();

    // T11: window level / window width
    void leftDrag_changesWindow();
    void leftDrag_widthNeverBelowOne();
    void leftDrag_keepsFunction();
    void leftDrag_onColor_pans();
    void setWindow_rerendersFrame();
    void setWindow_sameValue_doesNotEmit();
    void setWindow_sanitizes();
    void resetWindow_restoresDefault();
    void setWindow_onColor_isIgnored();
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

void TstImageViewport::wheelZoom_keepsPointUnderCursor() {
    ImageViewport viewport;
    QVERIFY(showViewport(viewport, 400, 300));
    viewport.setDocument(landscapeDocument());
    const QPointF cursor(100.0, 100.0);
    const QPointF before = viewport.widgetToImage(cursor);

    sendWheel(viewport, cursor, kWheelStep);
    QVERIFY(near(viewport.zoom(), 2.0 * kWheelZoomBase));
    QVERIFY(near(viewport.widgetToImage(cursor), before));
    QVERIFY(!viewport.isFitToWindow());
}

void TstImageViewport::wheelZoom_out() {
    ImageViewport viewport;
    QVERIFY(showViewport(viewport, 400, 300));
    viewport.setDocument(landscapeDocument());

    sendWheel(viewport, {200.0, 150.0}, -kWheelStep);
    QVERIFY(near(viewport.zoom(), 2.0 / kWheelZoomBase));
}

void TstImageViewport::zoom_isClamped() {
    constexpr double kMaxZoom = 16.0; // max(16 / dpr, fit * 4) with fit = 2
    constexpr double kMinZoom = 0.5;  // min(fit, 1 / dpr) * 0.5
    ImageViewport viewport;
    QVERIFY(showViewport(viewport, 400, 300));
    viewport.setDocument(landscapeDocument());

    for (int i = 0; i < 60; ++i) {
        sendWheel(viewport, {120.0, 80.0}, kWheelStep);
        QVERIFY(std::isfinite(viewport.zoom()));
    }
    QVERIFY(near(viewport.zoom(), kMaxZoom));
    for (int i = 0; i < 120; ++i) {
        sendWheel(viewport, {120.0, 80.0}, -kWheelStep);
        QVERIFY(std::isfinite(viewport.zoom()));
    }
    QVERIFY(near(viewport.zoom(), kMinZoom));
}

void TstImageViewport::zoomInOut_buttons() {
    ImageViewport viewport;
    QVERIFY(showViewport(viewport, 400, 300));
    viewport.setDocument(landscapeDocument());
    const QPointF center(200.0, 150.0);
    const QPointF before = viewport.widgetToImage(center);

    viewport.zoomIn();
    QVERIFY(near(viewport.zoom(), 2.0 * kButtonZoomStep));
    QVERIFY(near(viewport.widgetToImage(center), before));
    viewport.zoomOut();
    QVERIFY(near(viewport.zoom(), 2.0));
}

void TstImageViewport::actualSize() {
    ImageViewport viewport;
    QVERIFY(showViewport(viewport, 400, 300));
    viewport.setDocument(landscapeDocument());

    viewport.setActualSize();
    QVERIFY(near(viewport.zoom(), 1.0));
    QVERIFY(!viewport.isFitToWindow());
}

void TstImageViewport::doubleClick_fits() {
    ImageViewport viewport;
    QVERIFY(showViewport(viewport, 400, 300));
    viewport.setDocument(landscapeDocument());
    viewport.zoomIn();
    QVERIFY(!viewport.isFitToWindow());

    QTest::mouseDClick(&viewport, Qt::LeftButton, Qt::NoModifier, QPoint(200, 150));
    QVERIFY(viewport.isFitToWindow());
    QVERIFY(near(viewport.zoom(), 2.0));
}

void TstImageViewport::middleDrag_pans() {
    ImageViewport viewport;
    QVERIFY(showViewport(viewport, 400, 300));
    viewport.setDocument(landscapeDocument());
    const QPointF fixed(50.0, 25.0);
    const QPointF before = viewport.imageToWidget(fixed);

    QTest::mousePress(&viewport, Qt::MiddleButton, Qt::NoModifier, QPoint(200, 150));
    QTest::mouseMove(&viewport, QPoint(230, 170));
    QTest::mouseRelease(&viewport, Qt::MiddleButton, Qt::NoModifier, QPoint(230, 170));
    QVERIFY(near(viewport.imageToWidget(fixed) - before, QPointF(30.0, 20.0)));
    QVERIFY(!viewport.isFitToWindow());
}

void TstImageViewport::ctrlLeftDrag_pans() {
    ImageViewport viewport;
    QVERIFY(showViewport(viewport, 400, 300));
    viewport.setDocument(landscapeDocument());
    int windowSignals = 0;
    connect(&viewport, &ImageViewport::windowChanged, this, [&windowSignals] { ++windowSignals; });
    const QPointF fixed(50.0, 25.0);
    const QPointF before = viewport.imageToWidget(fixed);

    QTest::mousePress(&viewport, Qt::LeftButton, Qt::ControlModifier, QPoint(200, 150));
    QTest::mouseMove(&viewport, QPoint(230, 170));
    QTest::mouseRelease(&viewport, Qt::LeftButton, Qt::ControlModifier, QPoint(230, 170));
    QVERIFY(near(viewport.imageToWidget(fixed) - before, QPointF(30.0, 20.0)));
    QCOMPARE(windowSignals, 0);
}

void TstImageViewport::pan_isClamped() {
    ImageViewport viewport;
    QVERIFY(showViewport(viewport, 400, 300));
    viewport.setDocument(landscapeDocument());
    const QPoint center(200, 150);

    for (const QPoint target : {QPoint(10200, 10150), QPoint(-9800, -9850)}) {
        QTest::mousePress(&viewport, Qt::MiddleButton, Qt::NoModifier, center);
        QTest::mouseMove(&viewport, target);
        QTest::mouseRelease(&viewport, Qt::MiddleButton, Qt::NoModifier, target);
        const QPointF shown = viewport.widgetToImage(QPointF(center));
        QVERIFY(shown.x() >= 0.0 && shown.x() <= 200.0);
        QVERIFY(shown.y() >= 0.0 && shown.y() <= 100.0);
    }
}

void TstImageViewport::resizeAfterZoom_keepsZoom() {
    ImageViewport viewport;
    QVERIFY(showViewport(viewport, 400, 300));
    viewport.setDocument(landscapeDocument());
    viewport.zoomIn();
    const double zoom = viewport.zoom();

    viewport.resize(800, 600);
    QVERIFY(near(viewport.zoom(), zoom));
    QVERIFY(!viewport.isFitToWindow());
}

void TstImageViewport::probe_reportsValue() {
    ImageViewport viewport;
    QVERIFY(showViewport(viewport, 320, 320)); // the minimum width is 320
    viewport.setDocument(makeMonoDocument(2, 2, {0, 255, 128, 64}, kIdentityWindow));
    std::optional<PixelProbe> last;
    connect(&viewport, &ImageViewport::probeChanged, this,
            [&last](const std::optional<PixelProbe>& probe) { last = probe; });

    QTest::mouseMove(&viewport, viewport.imageToWidget({1.5, 0.5}).toPoint());
    QTRY_VERIFY(last.has_value());
    QCOMPARE(last->column, 1);
    QCOMPARE(last->row, 0);
    QCOMPARE(last->value, 255.0);
    QVERIFY(!last->isColor);
}

void TstImageViewport::probe_reportsColor() {
    ImageViewport viewport;
    QVERIFY(showViewport(viewport, 400, 300));
    viewport.setDocument(makeColorDocument(3, 1, {255, 0, 0, 0, 255, 0, 0, 0, 255}));
    std::optional<PixelProbe> last;
    connect(&viewport, &ImageViewport::probeChanged, this,
            [&last](const std::optional<PixelProbe>& probe) { last = probe; });

    QTest::mouseMove(&viewport, viewport.imageToWidget({1.5, 0.5}).toPoint());
    QTRY_VERIFY(last.has_value());
    QVERIFY(last->isColor);
    QCOMPARE(last->column, 1);
    QCOMPARE(last->rgb, qRgb(0, 255, 0));
}

void TstImageViewport::probe_outsideImage_isEmpty() {
    ImageViewport viewport;
    QVERIFY(showViewport(viewport, 400, 300));
    viewport.setDocument(makeMonoDocument(100, 200, filled(100, 200, 9), kIdentityWindow));
    std::optional<PixelProbe> last;
    int probeSignals = 0;
    connect(&viewport, &ImageViewport::probeChanged, this, [&](const std::optional<PixelProbe>& probe) {
        last = probe;
        ++probeSignals;
    });

    // Inside first, so the next signal is meaningful. Not the widget center: QTest skips a move to the
    // position where the previous test left the mouse.
    QTest::mouseMove(&viewport, QPoint(170, 110));
    QTRY_VERIFY(last.has_value());
    const int signalsInside = probeSignals;
    QTest::mouseMove(&viewport, QPoint(5, 5));
    QTRY_VERIFY(probeSignals > signalsInside);
    QVERIFY(!last.has_value());
}

void TstImageViewport::leftDrag_changesWindow() {
    ImageViewport viewport;
    QVERIFY(showViewport(viewport, 400, 300));
    viewport.setDocument(windowDragDocument());
    int windowSignals = 0;
    connect(&viewport, &ImageViewport::windowChanged, this, [&windowSignals] { ++windowSignals; });

    leftDrag(viewport, QPoint(100, 100), QPoint(150, 80));
    QCOMPARE(viewport.window(),
             (VoiWindow{.center = 960.0, .width = 600.0, .function = VoiFunction::Linear}));
    QVERIFY(windowSignals >= 1);
}

void TstImageViewport::leftDrag_widthNeverBelowOne() {
    ImageViewport viewport;
    QVERIFY(showViewport(viewport, 400, 300));
    viewport.setDocument(windowDragDocument());

    leftDrag(viewport, QPoint(300, 100), QPoint(-4700, 100));
    QCOMPARE(viewport.window().width, 1.0);
}

void TstImageViewport::leftDrag_keepsFunction() {
    constexpr VoiWindow kSigmoid{.center = 1000.0, .width = 500.0, .function = VoiFunction::Sigmoid};
    ImageViewport viewport;
    QVERIFY(showViewport(viewport, 400, 300));
    viewport.setDocument(windowDragDocument(kSigmoid));

    leftDrag(viewport, QPoint(100, 100), QPoint(140, 130));
    QVERIFY(viewport.window() != kSigmoid);
    QVERIFY(viewport.window().function == VoiFunction::Sigmoid);
}

void TstImageViewport::leftDrag_onColor_pans() {
    ImageViewport viewport;
    QVERIFY(showViewport(viewport, 400, 300));
    viewport.setDocument(makeColorDocument(2, 1, {255, 0, 0, 0, 0, 255}));
    int windowSignals = 0;
    connect(&viewport, &ImageViewport::windowChanged, this, [&windowSignals] { ++windowSignals; });
    const QPointF fixed(1.0, 0.5);
    const QPointF before = viewport.imageToWidget(fixed);

    leftDrag(viewport, QPoint(200, 150), QPoint(220, 160));
    QCOMPARE(windowSignals, 0);
    QVERIFY(near(viewport.imageToWidget(fixed) - before, QPointF(20.0, 10.0)));
}

void TstImageViewport::setWindow_rerendersFrame() {
    ImageViewport viewport;
    QVERIFY(showViewport(viewport, 400, 300));
    viewport.setDocument(makeMonoDocument(2, 2, {0, 255, 128, 64}, kIdentityWindow));
    QCOMPARE(grayAt(viewport.renderedFrame(), 0, 1), 128);

    viewport.setWindow({.center = 64.0, .width = 128.0, .function = VoiFunction::Linear});
    QCOMPARE(grayAt(viewport.renderedFrame(), 0, 1), 255);
}

void TstImageViewport::setWindow_sameValue_doesNotEmit() {
    ImageViewport viewport;
    QVERIFY(showViewport(viewport, 400, 300));
    viewport.setDocument(windowDragDocument());
    int windowSignals = 0;
    connect(&viewport, &ImageViewport::windowChanged, this, [&windowSignals] { ++windowSignals; });

    viewport.setWindow(viewport.window());
    QCOMPARE(windowSignals, 0);
}

void TstImageViewport::setWindow_sanitizes() {
    ImageViewport viewport;
    QVERIFY(showViewport(viewport, 400, 300));
    viewport.setDocument(windowDragDocument());

    viewport.setWindow({.center = 10.0, .width = -5.0, .function = VoiFunction::Linear});
    QCOMPARE(viewport.window().width, 1.0);
}

void TstImageViewport::resetWindow_restoresDefault() {
    ImageViewport viewport;
    QVERIFY(showViewport(viewport, 400, 300));
    viewport.setDocument(windowDragDocument());

    leftDrag(viewport, QPoint(100, 100), QPoint(170, 60));
    QVERIFY(viewport.window() != kDragWindow);
    viewport.resetWindow();
    QCOMPARE(viewport.window(), kDragWindow);
}

void TstImageViewport::setWindow_onColor_isIgnored() {
    ImageViewport viewport;
    QVERIFY(showViewport(viewport, 400, 300));
    viewport.setDocument(makeColorDocument(2, 1, {255, 0, 0, 0, 0, 255}));
    int windowSignals = 0;
    connect(&viewport, &ImageViewport::windowChanged, this, [&windowSignals] { ++windowSignals; });

    viewport.setWindow({.center = 10.0, .width = 20.0, .function = VoiFunction::Linear});
    viewport.resetWindow();
    QCOMPARE(windowSignals, 0);
    QVERIFY(!viewport.grab().isNull());
}

QTEST_MAIN(TstImageViewport)
#include "tst_ImageViewport.moc"
