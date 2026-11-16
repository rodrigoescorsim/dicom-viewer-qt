#pragma once

#include "app/DocumentLoader.hpp"
#include "core/LoadError.hpp"

#include <QMainWindow>
#include <QString>

#include <optional>

class QAction;
class QLabel;
class QToolBar;

namespace dv::app {

class ImageViewport;
class WindowLevelControls;
struct PixelProbe;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

    /// Starts loading `filePath` asynchronously (also used by main() and drag-and-drop).
    void openFile(const QString& filePath);
    [[nodiscard]] ImageViewport* viewport() const noexcept { return m_viewport; }

private slots:
    void chooseFile();
    void onLoadStarted(const QString& filePath);
    void onLoadSucceeded(const QString& filePath, const dv::app::DocumentPtr& document);
    void onLoadFailed(const QString& filePath, const dv::LoadError& error);
    void showProbe(const std::optional<dv::app::PixelProbe>& probe);
    void showZoom(double zoom);

private:
    void createActions();
    void createStatusBar();
    void updateActionStates();
    void updateImageInfo();
    /// Ends the busy state of every load started so far (superseded loads never report a result).
    void finishPendingLoads();

    ImageViewport* m_viewport{nullptr};
    DocumentLoader* m_loader{nullptr};
    QToolBar* m_toolBar{nullptr};
    QAction* m_openAction{nullptr};
    QAction* m_fitAction{nullptr};
    QAction* m_actualSizeAction{nullptr};
    QAction* m_zoomInAction{nullptr};
    QAction* m_zoomOutAction{nullptr};
    QAction* m_resetWindowAction{nullptr};
    WindowLevelControls* m_windowControls{nullptr};
    QLabel* m_probeLabel{nullptr};
    QLabel* m_imageInfoLabel{nullptr};
    QLabel* m_zoomLabel{nullptr};
    QString m_lastDirectory;
    int m_pendingLoads{0}; ///< Loads started whose busy cursor is still set.
};

} // namespace dv::app
