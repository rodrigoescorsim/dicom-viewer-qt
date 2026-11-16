#include "app/ImageViewport.hpp"

#include "core/GrayLut.hpp"
#include "core/ImageDocument.hpp"
#include "core/Raster.hpp"

#include <QColor>
#include <QFont>
#include <QPaintEvent>
#include <QPainter>
#include <QRectF>
#include <QResizeEvent>
#include <QSizeF>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace dv::app {

namespace {

constexpr int kMinimumWidth = 320;
constexpr int kMinimumHeight = 240;
constexpr qreal kTextMargin = 10.0;
constexpr qreal kEmptyStateFontIncrease = 2.0;
constexpr qreal kOrientationFontIncrease = 3.0;
constexpr QColor kEmptyStateColor{0x9A, 0x9A, 0x9A};
constexpr QColor kOrientationColor{0xF5, 0xC5, 0x42};

[[nodiscard]] QString toQString(const std::string& text) {
    return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

[[nodiscard]] QFont enlargedFont(QFont font, qreal pointIncrease, bool bold) {
    font.setPointSizeF(font.pointSizeF() + pointIncrease);
    font.setBold(bold);
    return font;
}

} // namespace

ImageViewport::ImageViewport(QWidget* parent)
    : QWidget(parent) {
    setAttribute(Qt::WA_OpaquePaintEvent);
    setMinimumSize(kMinimumWidth, kMinimumHeight);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
}

void ImageViewport::setDocument(DocumentPtr document) {
    m_frame = QImage(); // must be released BEFORE the old document: a color frame points into its memory
    m_document = std::move(document);
    m_frameLevel = -1;
    m_frameDirty = true;
    if (m_document) {
        if (const dv::MonoImage* mono = m_document->mono()) {
            m_window = mono->defaultWindow;
        }
        fitToWindow(); // emits zoomChanged
        emit windowChanged(m_window);
    }
    emit probeChanged(std::nullopt);
    update();
}

double ImageViewport::zoom() const noexcept {
    return m_scale * devicePixelRatioF();
}

QPointF ImageViewport::imageToWidget(QPointF imagePoint) const noexcept {
    return {width() / 2.0 + (imagePoint.x() - m_center.x()) * scaleX(),
            height() / 2.0 + (imagePoint.y() - m_center.y()) * scaleY()};
}

QPointF ImageViewport::widgetToImage(QPointF widgetPoint) const noexcept {
    return {m_center.x() + (widgetPoint.x() - width() / 2.0) / scaleX(),
            m_center.y() + (widgetPoint.y() - height() / 2.0) / scaleY()};
}

const QImage& ImageViewport::renderedFrame() {
    if (m_document) {
        ensureFrame();
    }
    return m_frame;
}

void ImageViewport::setWindow(const dv::VoiWindow& window) {
    if (!m_document || m_document->mono() == nullptr) {
        return; // color images have no VOI window
    }
    const dv::VoiWindow clean = dv::sanitized(window);
    if (clean == m_window) {
        return; // also breaks the viewport <-> controls signal loop
    }
    m_window = clean;
    m_frameDirty = true;
    update();
    emit windowChanged(m_window);
}

void ImageViewport::resetWindow() {
    if (m_document && m_document->mono() != nullptr) {
        setWindow(m_document->mono()->defaultWindow);
    }
}

void ImageViewport::fitToWindow() {
    m_fitToWindow = true;
    if (m_document) {
        m_scale = fitScale();
        m_center = QPointF(m_document->width / 2.0, m_document->height / 2.0);
    }
    update();
    emit zoomChanged(zoom());
}

void ImageViewport::setActualSize() {}

void ImageViewport::zoomIn() {}

void ImageViewport::zoomOut() {}

void ImageViewport::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.fillRect(rect(), Qt::black); // viewer background is always black, whatever the theme
    if (!m_document) {
        drawEmptyState(painter);
        return;
    }
    ensureFrame();
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    const QRectF target(imageToWidget({0.0, 0.0}),
                        QSizeF(m_document->width * scaleX(), m_document->height * scaleY()));
    painter.drawImage(target, m_frame, QRectF(m_frame.rect()));
    drawOrientationLabels(painter);
}

void ImageViewport::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    if (m_fitToWindow && m_document) {
        fitToWindow();
    }
}

double ImageViewport::scaleX() const noexcept {
    return m_scale;
}

double ImageViewport::scaleY() const noexcept {
    return m_document ? m_scale * m_document->pixelAspectRatio : m_scale;
}

double ImageViewport::fitScale() const noexcept {
    if (!m_document || width() <= 0 || height() <= 0) {
        return 1.0;
    }
    return std::min(width() / static_cast<double>(m_document->width),
                    height() / (m_document->height * m_document->pixelAspectRatio));
}

void ImageViewport::ensureFrame() {
    const int level =
        dv::selectPyramidLevel(std::min(scaleX(), scaleY()) * devicePixelRatioF(), m_document->levelCount());
    if (!m_frameDirty && level == m_frameLevel) {
        return;
    }
    if (const dv::MonoImage* mono = m_document->mono()) {
        const dv::GrayRaster& raster = mono->levels[static_cast<std::size_t>(level)];
        if (m_frame.format() != QImage::Format_Grayscale8 || m_frame.width() != raster.width ||
            m_frame.height() != raster.height) {
            m_frame = QImage(raster.width, raster.height, QImage::Format_Grayscale8);
        }
        const std::vector<std::uint8_t> lut = dv::buildGrayLut({.window = m_window,
                                                                .mapping = mono->mapping,
                                                                .entryCount = mono->lutEntryCount,
                                                                .inverted = mono->inverted,
                                                                .padding = mono->padding});
        // QImage rows are 32-bit aligned: always pass bytesPerLine(), never assume stride == width.
        dv::applyGrayLut(raster.data, lut, raster.width, raster.height, m_frame.bits(),
                         m_frame.bytesPerLine());
    } else if (const dv::ColorImage* color = m_document->color()) {
        const dv::RgbRaster& raster = color->levels[static_cast<std::size_t>(level)];
        // Non-owning view with an explicit stride (width * 3, no row padding). m_document keeps it alive.
        m_frame = QImage(raster.data.data(), raster.width, raster.height,
                         static_cast<qsizetype>(raster.width) * 3, QImage::Format_RGB888);
    }
    m_frameLevel = level;
    m_frameDirty = false;
}

void ImageViewport::drawEmptyState(QPainter& painter) const {
    painter.setFont(enlargedFont(font(), kEmptyStateFontIncrease, false));
    painter.setPen(kEmptyStateColor);
    painter.drawText(rect(), Qt::AlignCenter,
                     tr("Abra um arquivo DICOM (Ctrl+O)") + QLatin1Char('\n') +
                         tr("ou arraste-o para esta janela"));
}

void ImageViewport::drawOrientationLabels(QPainter& painter) const {
    const dv::EdgeLabels& labels = m_document->orientation;
    if (labels.empty()) {
        return;
    }
    painter.save();
    painter.setFont(enlargedFont(font(), kOrientationFontIncrease, true));
    const QRectF area = QRectF(rect()).adjusted(kTextMargin, kTextMargin, -kTextMargin, -kTextMargin);
    const std::array<std::pair<const std::string*, int>, 4> edges{{
        {&labels.left, Qt::AlignLeft | Qt::AlignVCenter},
        {&labels.right, Qt::AlignRight | Qt::AlignVCenter},
        {&labels.top, Qt::AlignTop | Qt::AlignHCenter},
        {&labels.bottom, Qt::AlignBottom | Qt::AlignHCenter},
    }};
    for (const auto& [label, flags] : edges) {
        if (!label->empty()) {
            drawOutlinedText(painter, area, flags, toQString(*label), kOrientationColor);
        }
    }
    painter.restore();
}

void ImageViewport::drawOutlinedText(QPainter& painter, const QRectF& rect, int flags, const QString& text,
                                     const QColor& color) {
    // A black outline keeps the text readable over any image content.
    constexpr std::array kOutlineOffsets{QPointF(-1.0, -1.0), QPointF(1.0, -1.0), QPointF(-1.0, 1.0),
                                         QPointF(1.0, 1.0)};
    painter.setPen(Qt::black);
    for (const QPointF& offset : kOutlineOffsets) {
        painter.drawText(rect.translated(offset), flags, text);
    }
    painter.setPen(color);
    painter.drawText(rect, flags, text);
}

} // namespace dv::app
