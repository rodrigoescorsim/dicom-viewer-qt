#pragma once

#include "app/DocumentLoader.hpp" // DocumentPtr
#include "core/Voi.hpp"

#include <QImage>
#include <QPointF>
#include <QRgb>
#include <QWidget>

#include <optional>

class QColor;
class QPainter;
class QRectF;

namespace dv::app {

/// What lies under the mouse cursor.
struct PixelProbe {
    int column{0};
    int row{0};
    bool isColor{false};
    double value{0.0}; ///< Modality value (monochrome images).
    QRgb rgb{0};       ///< Color images.
};

/// Displays one ImageDocument with the right size, aspect ratio, gray scale and orientation (row 0 at the
/// top, column 0 at the left; never mirrored nor rotated).
class ImageViewport : public QWidget {
    Q_OBJECT

public:
    explicit ImageViewport(QWidget* parent = nullptr);

    /// Shows `document` fitted to the widget with its default window. nullptr shows the empty state.
    void setDocument(DocumentPtr document);
    [[nodiscard]] const DocumentPtr& document() const noexcept { return m_document; }

    [[nodiscard]] dv::VoiWindow window() const noexcept { return m_window; }
    /// Device pixels per image column. 1.0 means one image pixel per screen pixel ("100 %").
    [[nodiscard]] double zoom() const noexcept;
    [[nodiscard]] bool isFitToWindow() const noexcept { return m_fitToWindow; }

    /// Image coordinates are continuous: pixel (column, row) covers [column, column+1) x [row, row+1).
    [[nodiscard]] QPointF imageToWidget(QPointF imagePoint) const noexcept;
    [[nodiscard]] QPointF widgetToImage(QPointF widgetPoint) const noexcept;

    /// The frame that paintEvent() draws for the current state (pyramid level + window). For tests.
    [[nodiscard]] const QImage& renderedFrame();

public slots:
    void setWindow(const dv::VoiWindow& window);
    void resetWindow();
    void fitToWindow();
    void setActualSize();
    void zoomIn();
    void zoomOut();

signals:
    void windowChanged(const dv::VoiWindow& window);
    void zoomChanged(double zoom);
    void probeChanged(const std::optional<dv::app::PixelProbe>& probe);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    [[nodiscard]] double scaleX() const noexcept; // logical px per image column  (= m_scale)
    [[nodiscard]] double scaleY() const noexcept; // logical px per image row     (= m_scale * aspect)
    [[nodiscard]] double fitScale() const noexcept;
    void ensureFrame();
    void drawEmptyState(QPainter& painter) const;
    void drawOrientationLabels(QPainter& painter) const;
    static void drawOutlinedText(QPainter& painter, const QRectF& rect, int flags, const QString& text,
                                 const QColor& color);

    DocumentPtr m_document;
    dv::VoiWindow m_window;
    double m_scale{1.0}; ///< Logical widget pixels per image column.
    QPointF m_center;    ///< Image point shown at the center of the widget.
    bool m_fitToWindow{true};

    QImage m_frame; ///< Cache: current pyramid level rendered with the current window.
    int m_frameLevel{-1};
    bool m_frameDirty{true};
};

} // namespace dv::app
