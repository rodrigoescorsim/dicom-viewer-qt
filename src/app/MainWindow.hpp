#pragma once

#include "app/DocumentLoader.hpp"
#include "core/LoadError.hpp"

#include <QMainWindow>
#include <QString>

class QAction;

namespace dv::app {

class ImageViewport;

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

private:
    void createActions();
    /// Ends the busy state of every load started so far (superseded loads never report a result).
    void finishPendingLoads();

    ImageViewport* m_viewport{nullptr};
    DocumentLoader* m_loader{nullptr};
    QAction* m_openAction{nullptr};
    QString m_lastDirectory;
    int m_pendingLoads{0}; ///< Loads started whose busy cursor is still set.
};

} // namespace dv::app
