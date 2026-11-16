#pragma once

#include "app/DocumentLoader.hpp"
#include "core/LoadError.hpp"

#include <QMainWindow>
#include <QString>

#include <optional>

class QAction;
class QLabel;
class QProgressBar;
class QToolBar;

namespace dv::app {

class ImageViewport;
class MetadataPanel;
class WindowLevelControls;
struct PixelProbe;

/// Application window: opens files, shows the viewport, the window controls, the metadata dock and the
/// status bar, and reports load errors.
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

    /// Starts loading `filePath` asynchronously (also used by main() and drag-and-drop).
    void openFile(const QString& filePath);
    [[nodiscard]] ImageViewport* viewport() const noexcept { return m_viewport; }

protected:
    void closeEvent(QCloseEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private slots:
    void chooseFile();
    void showAbout();
    void onLoadStarted(const QString& filePath);
    void onLoadSucceeded(const QString& filePath, const dv::app::DocumentPtr& document);
    void onLoadFailed(const QString& filePath, const dv::LoadError& error);
    void showProbe(const std::optional<dv::app::PixelProbe>& probe);
    void showZoom(double zoom);

private:
    void createMetadataDock();
    void createActions(); ///< Menus and their actions. Needs the metadata dock.
    void createToolBar(); ///< Needs the actions.
    void createStatusBar();
    void restoreSettings();
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
    QAction* m_metadataAction{nullptr}; ///< Owned by the metadata dock (its toggleViewAction).
    WindowLevelControls* m_windowControls{nullptr};
    MetadataPanel* m_metadataPanel{nullptr};
    QLabel* m_probeLabel{nullptr};
    QLabel* m_imageInfoLabel{nullptr};
    QLabel* m_zoomLabel{nullptr};
    QProgressBar* m_loadProgress{nullptr};
    QString m_lastDirectory;
    int m_pendingLoads{0}; ///< Loads started whose busy cursor is still set.
};

} // namespace dv::app
