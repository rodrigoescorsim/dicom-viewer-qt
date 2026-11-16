#include "app/MainWindow.hpp"

#include "app/ErrorMessages.hpp"
#include "app/ImageViewport.hpp"
#include "app/MetadataPanel.hpp"
#include "app/WindowLevelControls.hpp"
#include "core/ImageDocument.hpp"
#include "dicom/DicomLoader.hpp"

#include <QAction>
#include <QCloseEvent>
#include <QDir>
#include <QDockWidget>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QIcon>
#include <QKeySequence>
#include <QLabel>
#include <QList>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QProgressBar>
#include <QSettings>
#include <QStandardPaths>
#include <QStatusBar>
#include <QToolBar>
#include <QUrl>

#include <string>
#include <string_view>

namespace dv::app {

namespace {

constexpr int kInitialWidth = 1280;
constexpr int kInitialHeight = 800;
constexpr int kMinimumWidth = 900;
constexpr int kMinimumHeight = 600;
constexpr double kPercent = 100.0;
constexpr int kMetadataDockWidth = 440;
constexpr int kProgressBarMaximumWidth = 140;
constexpr int kMultiFrameNoticeMs = 10000;

constexpr auto kGeometryKey = "mainWindow/geometry";
constexpr auto kStateKey = "mainWindow/state";
constexpr auto kLastDirectoryKey = "files/lastDirectory";

[[nodiscard]] QString toQString(std::string_view text) {
    return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

/// "Ajustar à janela (Ctrl+0)": the action text without mnemonic and ellipsis, plus its shortcut.
void setToolTipWithShortcut(QAction* action) {
    QString text = action->text();
    text.remove(QLatin1Char('&')).remove(QChar(0x2026));
    const QKeySequence shortcut = action->shortcut();
    action->setToolTip(shortcut.isEmpty() ? text
                                          : QStringLiteral("%1 (%2)").arg(
                                                text, shortcut.toString(QKeySequence::NativeText)));
}

/// The first URL of a drag, if it is a local file.
[[nodiscard]] QString droppedFile(const QMimeData* mimeData) {
    if (mimeData == nullptr || !mimeData->hasUrls() || mimeData->urls().isEmpty()) {
        return {};
    }
    const QUrl url = mimeData->urls().constFirst();
    return url.isLocalFile() ? url.toLocalFile() : QString();
}

} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_viewport(new ImageViewport(this))
    , m_loader(new DocumentLoader(this)) {
    setWindowTitle(tr("DICOM Viewer"));
    resize(kInitialWidth, kInitialHeight);
    setMinimumSize(kMinimumWidth, kMinimumHeight);
    setCentralWidget(m_viewport);
    setAcceptDrops(true);

    connect(m_loader, &DocumentLoader::loadStarted, this, &MainWindow::onLoadStarted);
    connect(m_loader, &DocumentLoader::loadSucceeded, this, &MainWindow::onLoadSucceeded);
    connect(m_loader, &DocumentLoader::loadFailed, this, &MainWindow::onLoadFailed);

    createMetadataDock();
    createActions();
    createToolBar();
    createStatusBar();
    connect(m_viewport, &ImageViewport::probeChanged, this, &MainWindow::showProbe);
    connect(m_viewport, &ImageViewport::zoomChanged, this, &MainWindow::showZoom);
    restoreSettings();
    updateActionStates();
}

void MainWindow::openFile(const QString& filePath) {
    m_lastDirectory = QFileInfo(filePath).absolutePath();
    QSettings().setValue(kLastDirectoryKey, m_lastDirectory);
    m_loader->load(filePath);
}

void MainWindow::closeEvent(QCloseEvent* event) {
    QSettings settings;
    settings.setValue(kGeometryKey, saveGeometry());
    settings.setValue(kStateKey, saveState());
    QMainWindow::closeEvent(event);
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event) {
    if (!droppedFile(event->mimeData()).isEmpty()) {
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent* event) {
    if (const QString filePath = droppedFile(event->mimeData()); !filePath.isEmpty()) {
        event->acceptProposedAction();
        openFile(filePath);
    }
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

    viewMenu->addSeparator();
    m_resetWindowAction =
        viewMenu->addAction(QIcon::fromTheme(QIcon::ThemeIcon::ViewRefresh), tr("Restaurar janela"));
    m_resetWindowAction->setObjectName(QStringLiteral("resetWindowAction"));
    m_resetWindowAction->setShortcut(QKeySequence(Qt::Key_R));
    connect(m_resetWindowAction, &QAction::triggered, m_viewport, &ImageViewport::resetWindow);

    viewMenu->addSeparator();
    viewMenu->addAction(m_metadataAction);

    QMenu* helpMenu = menuBar()->addMenu(tr("Aj&uda"));
    QAction* aboutAction = helpMenu->addAction(tr("&Sobre…"));
    connect(aboutAction, &QAction::triggered, this, &MainWindow::showAbout);
}

void MainWindow::createMetadataDock() {
    m_metadataPanel = new MetadataPanel(this);
    auto* metadataDock = new QDockWidget(tr("Metadados DICOM"), this);
    metadataDock->setObjectName(QStringLiteral("metadataDock"));
    metadataDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    metadataDock->setWidget(m_metadataPanel);
    addDockWidget(Qt::RightDockWidgetArea, metadataDock);
    resizeDocks({metadataDock}, {kMetadataDockWidth}, Qt::Horizontal);

    m_metadataAction = metadataDock->toggleViewAction();
    m_metadataAction->setText(tr("Metadados"));
    m_metadataAction->setShortcut(QKeySequence(Qt::Key_F9));
}

void MainWindow::createToolBar() {
    m_windowControls = new WindowLevelControls(this);
    // setWindow() on both sides ignores unchanged values, which ends the signal round trip.
    connect(m_viewport, &ImageViewport::windowChanged, m_windowControls, &WindowLevelControls::setWindow);
    connect(m_windowControls, &WindowLevelControls::windowEdited, m_viewport, &ImageViewport::setWindow);

    // Order: Open | zoom | window | metadata.
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
    m_toolBar->addSeparator();
    m_toolBar->addWidget(m_windowControls);
    m_toolBar->addAction(m_resetWindowAction);
    m_toolBar->addSeparator();
    m_toolBar->addAction(m_metadataAction);

    for (QAction* action : {m_openAction, m_fitAction, m_actualSizeAction, m_zoomInAction, m_zoomOutAction,
                            m_resetWindowAction, m_metadataAction}) {
        setToolTipWithShortcut(action);
    }
}

void MainWindow::createStatusBar() {
    m_probeLabel = new QLabel(this);
    m_imageInfoLabel = new QLabel(this);
    m_zoomLabel = new QLabel(this);
    m_loadProgress = new QProgressBar(this);
    m_loadProgress->setRange(0, 0); // indeterminate
    m_loadProgress->setTextVisible(false);
    m_loadProgress->setMaximumWidth(kProgressBarMaximumWidth);
    m_loadProgress->setVisible(false);
    statusBar()->addWidget(m_probeLabel);
    statusBar()->addPermanentWidget(m_loadProgress);
    statusBar()->addPermanentWidget(m_imageInfoLabel);
    statusBar()->addPermanentWidget(m_zoomLabel);
}

void MainWindow::restoreSettings() {
    const QSettings settings;
    if (const QByteArray geometry = settings.value(kGeometryKey).toByteArray(); !geometry.isEmpty()) {
        restoreGeometry(geometry);
    }
    if (const QByteArray state = settings.value(kStateKey).toByteArray(); !state.isEmpty()) {
        restoreState(state);
    }
    m_lastDirectory =
        settings.value(kLastDirectoryKey, QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation))
            .toString();
}

void MainWindow::updateActionStates() {
    const DocumentPtr& document = m_viewport->document();
    const bool hasDocument = document != nullptr;
    const bool isMono = hasDocument && document->mono() != nullptr;
    for (QAction* action : {m_fitAction, m_actualSizeAction, m_zoomInAction, m_zoomOutAction}) {
        action->setEnabled(hasDocument);
    }
    m_resetWindowAction->setEnabled(isMono);
    m_windowControls->setEnabled(isMono);
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
    openFile(filePath);
}

void MainWindow::showAbout() {
    QMessageBox::about(this, tr("Sobre o %1").arg(QCoreApplication::applicationName()),
                       tr("<h3>%1 %2</h3>"
                          "<p>Teste técnico — Visualização DICOM com Qt</p>"
                          "<p>Qt %3<br>DCMTK %4</p>")
                           .arg(QCoreApplication::applicationName(), QCoreApplication::applicationVersion(),
                                QString::fromLatin1(qVersion()), toQString(dv::dicomToolkitVersion())));
}

void MainWindow::onLoadStarted(const QString& filePath) {
    ++m_pendingLoads;
    QGuiApplication::setOverrideCursor(Qt::BusyCursor);
    m_loadProgress->setVisible(true);
    statusBar()->showMessage(tr("Carregando %1…").arg(QFileInfo(filePath).fileName()));
}

void MainWindow::onLoadSucceeded(const QString& filePath, const dv::app::DocumentPtr& document) {
    finishPendingLoads();
    m_windowControls->setDocument(document); // first: the viewport's initial windowChanged needs the presets
    m_viewport->setDocument(document);
    m_metadataPanel->setMetadata(document->metadata);
    setWindowTitle(tr("%1 — DICOM Viewer").arg(QFileInfo(filePath).fileName()));
    updateImageInfo();
    updateActionStates();
    if (const int frames = document->info.frameCount; frames > 1) {
        statusBar()->showMessage(tr("Arquivo multi-frame: exibindo o quadro 1 de %1.").arg(frames),
                                 kMultiFrameNoticeMs);
    }
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
    m_loadProgress->setVisible(false);
    statusBar()->clearMessage();
}

} // namespace dv::app
