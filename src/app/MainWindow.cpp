#include "app/MainWindow.hpp"

#include "app/ErrorMessages.hpp"
#include "app/ImageViewport.hpp"
#include "app/WindowLevelControls.hpp"
#include "core/ImageDocument.hpp"

#include <QAction>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QIcon>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QStatusBar>
#include <QToolBar>

#include <string>

namespace dv::app {

namespace {

constexpr int kInitialWidth = 1280;
constexpr int kInitialHeight = 800;
constexpr double kPercent = 100.0;

[[nodiscard]] QString toQString(const std::string& text) {
    return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_viewport(new ImageViewport(this))
    , m_loader(new DocumentLoader(this)) {
    setWindowTitle(tr("DICOM Viewer"));
    resize(kInitialWidth, kInitialHeight);
    setCentralWidget(m_viewport);

    connect(m_loader, &DocumentLoader::loadStarted, this, &MainWindow::onLoadStarted);
    connect(m_loader, &DocumentLoader::loadSucceeded, this, &MainWindow::onLoadSucceeded);
    connect(m_loader, &DocumentLoader::loadFailed, this, &MainWindow::onLoadFailed);

    createActions();
    createStatusBar();
    connect(m_viewport, &ImageViewport::probeChanged, this, &MainWindow::showProbe);
    connect(m_viewport, &ImageViewport::zoomChanged, this, &MainWindow::showZoom);
    updateActionStates();
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

    QMenu* viewMenu = menuBar()->addMenu(tr("&Exibir"));
    m_fitAction =
        viewMenu->addAction(QIcon::fromTheme(QIcon::ThemeIcon::ZoomFitBest), tr("Ajustar à janela"));
    m_fitAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_0));
    connect(m_fitAction, &QAction::triggered, m_viewport, &ImageViewport::fitToWindow);

    m_actualSizeAction = viewMenu->addAction(tr("Tamanho real (100 %)"));
    m_actualSizeAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_1));
    connect(m_actualSizeAction, &QAction::triggered, m_viewport, &ImageViewport::setActualSize);

    m_zoomInAction = viewMenu->addAction(QIcon::fromTheme(QIcon::ThemeIcon::ZoomIn), tr("Ampliar"));
    m_zoomInAction->setShortcut(QKeySequence::ZoomIn);
    connect(m_zoomInAction, &QAction::triggered, m_viewport, &ImageViewport::zoomIn);

    m_zoomOutAction = viewMenu->addAction(QIcon::fromTheme(QIcon::ThemeIcon::ZoomOut), tr("Reduzir"));
    m_zoomOutAction->setShortcut(QKeySequence::ZoomOut);
    connect(m_zoomOutAction, &QAction::triggered, m_viewport, &ImageViewport::zoomOut);

    m_toolBar = addToolBar(tr("Barra de ferramentas"));
    m_toolBar->setObjectName(QStringLiteral("mainToolBar"));
    m_toolBar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_toolBar->setFloatable(false);
    m_toolBar->addAction(m_openAction);
    m_toolBar->addSeparator();
    m_toolBar->addAction(m_fitAction);
    m_toolBar->addAction(m_actualSizeAction);
    m_toolBar->addAction(m_zoomInAction);
    m_toolBar->addAction(m_zoomOutAction);

    m_windowControls = new WindowLevelControls(this);
    m_resetWindowAction =
        viewMenu->addAction(QIcon::fromTheme(QIcon::ThemeIcon::ViewRefresh), tr("Restaurar janela"));
    m_resetWindowAction->setShortcut(QKeySequence(Qt::Key_R));
    connect(m_resetWindowAction, &QAction::triggered, m_viewport, &ImageViewport::resetWindow);
    m_toolBar->addSeparator();
    m_toolBar->addWidget(m_windowControls);
    m_toolBar->addAction(m_resetWindowAction);

    // setWindow() on both sides ignores unchanged values, which ends the signal round trip.
    connect(m_viewport, &ImageViewport::windowChanged, m_windowControls, &WindowLevelControls::setWindow);
    connect(m_windowControls, &WindowLevelControls::windowEdited, m_viewport, &ImageViewport::setWindow);
}

void MainWindow::createStatusBar() {
    m_probeLabel = new QLabel(this);
    m_imageInfoLabel = new QLabel(this);
    m_zoomLabel = new QLabel(this);
    statusBar()->addWidget(m_probeLabel);
    statusBar()->addPermanentWidget(m_imageInfoLabel);
    statusBar()->addPermanentWidget(m_zoomLabel);
}

void MainWindow::updateActionStates() {
    const bool hasDocument = m_viewport->document() != nullptr;
    for (QAction* action : {m_fitAction, m_actualSizeAction, m_zoomInAction, m_zoomOutAction}) {
        action->setEnabled(hasDocument);
    }
    m_resetWindowAction->setEnabled(hasDocument && m_viewport->document()->mono() != nullptr);
}

void MainWindow::updateImageInfo() {
    const DocumentPtr& document = m_viewport->document();
    if (!document) {
        m_imageInfoLabel->clear();
        return;
    }
    m_imageInfoLabel->setText(tr("%1 × %2 px  ·  %3 bits  ·  %4  ·  %5")
                                  .arg(document->width)
                                  .arg(document->height)
                                  .arg(document->info.bitsStored)
                                  .arg(toQString(document->info.photometricInterpretation),
                                       toQString(document->info.transferSyntaxName)));
}

void MainWindow::showProbe(const std::optional<dv::app::PixelProbe>& probe) {
    if (!probe) {
        m_probeLabel->clear();
        return;
    }
    if (probe->isColor) {
        m_probeLabel->setText(tr("X: %1  Y: %2  RGB: (%3, %4, %5)")
                                  .arg(probe->column)
                                  .arg(probe->row)
                                  .arg(qRed(probe->rgb))
                                  .arg(qGreen(probe->rgb))
                                  .arg(qBlue(probe->rgb)));
        return;
    }
    const dv::MonoImage* mono = m_viewport->document() ? m_viewport->document()->mono() : nullptr;
    const int decimals = (mono == nullptr || mono->mapping.scale == 1.0) ? 0 : 1;
    m_probeLabel->setText(tr("X: %1  Y: %2  Valor: %3")
                              .arg(probe->column)
                              .arg(probe->row)
                              .arg(QString::number(probe->value, 'f', decimals)));
}

void MainWindow::showZoom(double zoom) {
    m_zoomLabel->setText(m_viewport->document() ? tr("%1 %").arg(qRound(zoom * kPercent)) : QString());
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
    m_windowControls->setDocument(document); // first: the viewport's initial windowChanged needs the presets
    m_viewport->setDocument(document);
    setWindowTitle(tr("%1 — DICOM Viewer").arg(QFileInfo(filePath).fileName()));
    updateImageInfo();
    updateActionStates();
}

void MainWindow::onLoadFailed(const QString& filePath, const dv::LoadError& error) {
    finishPendingLoads();
    // The image that was open stays open: a failed load never clears the viewport.
    auto* box = new QMessageBox(QMessageBox::Critical, tr("Não foi possível abrir o arquivo"),
                                describeLoadError(error), QMessageBox::Ok, this);
    box->setInformativeText(QDir::toNativeSeparators(filePath));
    if (!error.detail.empty()) {
        box->setDetailedText(toQString(error.detail));
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
