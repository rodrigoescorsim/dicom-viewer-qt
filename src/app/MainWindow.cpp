#include "app/MainWindow.hpp"

#include "app/ErrorMessages.hpp"
#include "app/ImageViewport.hpp"

#include <QAction>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QIcon>
#include <QKeySequence>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QStatusBar>

namespace dv::app {

namespace {

constexpr int kInitialWidth = 1280;
constexpr int kInitialHeight = 800;

} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_viewport(new ImageViewport(this))
    , m_loader(new DocumentLoader(this)) {
    setWindowTitle(tr("DICOM Viewer"));
    resize(kInitialWidth, kInitialHeight);
    setCentralWidget(m_viewport);
    statusBar(); // created up front so the first message does not change the layout

    connect(m_loader, &DocumentLoader::loadStarted, this, &MainWindow::onLoadStarted);
    connect(m_loader, &DocumentLoader::loadSucceeded, this, &MainWindow::onLoadSucceeded);
    connect(m_loader, &DocumentLoader::loadFailed, this, &MainWindow::onLoadFailed);

    createActions();
}

void MainWindow::openFile(const QString& filePath) {
    m_loader->load(filePath);
}

void MainWindow::createActions() {
    QMenu* fileMenu = menuBar()->addMenu(tr("&Arquivo"));

    m_openAction = fileMenu->addAction(QIcon::fromTheme(QIcon::ThemeIcon::DocumentOpen), tr("&Abrir…"));
    m_openAction->setShortcut(QKeySequence::Open);
    connect(m_openAction, &QAction::triggered, this, &MainWindow::chooseFile);

    fileMenu->addSeparator();
    QAction* quitAction = fileMenu->addAction(tr("&Sair"));
    quitAction->setShortcut(QKeySequence::Quit);
    connect(quitAction, &QAction::triggered, this, &QWidget::close);
}

void MainWindow::chooseFile() {
    // DICOM files often have no extension at all: "Todos os arquivos" must stay available.
    const QString filePath =
        QFileDialog::getOpenFileName(this, tr("Abrir arquivo DICOM"), m_lastDirectory,
                                     tr("Arquivos DICOM (*.dcm *.dicom *.dic);;Todos os arquivos (*)"));
    if (filePath.isEmpty()) {
        return; // cancelled
    }
    m_lastDirectory = QFileInfo(filePath).absolutePath();
    openFile(filePath);
}

void MainWindow::onLoadStarted(const QString& filePath) {
    ++m_pendingLoads;
    QGuiApplication::setOverrideCursor(Qt::BusyCursor);
    statusBar()->showMessage(tr("Carregando %1…").arg(QFileInfo(filePath).fileName()));
}

void MainWindow::onLoadSucceeded(const QString& filePath, const dv::app::DocumentPtr& document) {
    finishPendingLoads();
    m_viewport->setDocument(document);
    setWindowTitle(tr("%1 — DICOM Viewer").arg(QFileInfo(filePath).fileName()));
}

void MainWindow::onLoadFailed(const QString& filePath, const dv::LoadError& error) {
    finishPendingLoads();
    // The image that was open stays open: a failed load never clears the viewport.
    auto* box = new QMessageBox(QMessageBox::Critical, tr("Não foi possível abrir o arquivo"),
                                describeLoadError(error), QMessageBox::Ok, this);
    box->setInformativeText(QDir::toNativeSeparators(filePath));
    if (!error.detail.empty()) {
        box->setDetailedText(
            QString::fromUtf8(error.detail.data(), static_cast<qsizetype>(error.detail.size())));
    }
    box->setAttribute(Qt::WA_DeleteOnClose);
    box->open(); // window-modal and non-blocking: no nested event loop
}

void MainWindow::finishPendingLoads() {
    for (; m_pendingLoads > 0; --m_pendingLoads) {
        QGuiApplication::restoreOverrideCursor();
    }
    statusBar()->clearMessage();
}

} // namespace dv::app
