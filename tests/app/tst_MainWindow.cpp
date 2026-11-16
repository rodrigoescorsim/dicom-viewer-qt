#include "app/ErrorMessages.hpp"
#include "app/ImageViewport.hpp"
#include "app/MainWindow.hpp"
#include "app/WindowLevelControls.hpp"
#include "dicom/CodecRegistry.hpp"

#include <QAction>
#include <QApplication>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileInfo>
#include <QMessageBox>
#include <QMimeData>
#include <QSettings>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTest>
#include <QUrl>

#include <memory>
#include <string_view>

#include "support/TestSupport.hpp"

using dv::app::MainWindow;

namespace {

constexpr int kLoadTimeoutMs = 10000;
constexpr int kSmallSide = 64;

[[nodiscard]] QString sampleFile(std::string_view fileName) {
    return QString::fromStdU16String(dv::test::samplePath(fileName).u16string());
}

[[nodiscard]] QAction* findAction(const MainWindow& window, const QString& text) {
    for (QAction* action : window.findChildren<QAction*>()) {
        if (action->text() == text) {
            return action;
        }
    }
    return nullptr;
}

[[nodiscard]] QMessageBox* visibleMessageBox(const MainWindow& window) {
    for (QMessageBox* box : window.findChildren<QMessageBox*>()) {
        if (box->isVisible()) {
            return box;
        }
    }
    return nullptr;
}

/// Sends a drag-enter carrying `mimeData` and returns whether the window accepted it.
[[nodiscard]] bool sendDragEnter(MainWindow& window, QMimeData& mimeData) {
    const QPoint position = window.rect().center();
    QDragEnterEvent event(position, Qt::CopyAction, &mimeData, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&window, &event);
    return event.isAccepted();
}

} // namespace

class TstMainWindow : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void startsEmpty();
    void openFile_showsImage();
    void openInvalidFile_showsErrorAndKeepsImage();
    void colorImage_disablesWindowControls();
    void multiFrame_showsNotice();
    void dropFile_opensIt();
    void dropNonFile_isIgnored();
    void rememberLastDirectory();

private:
    /// Creates, shows and returns a fresh window.
    [[nodiscard]] static std::unique_ptr<MainWindow> showWindow();

    std::unique_ptr<dv::CodecRegistry> m_codecs;
};

void TstMainWindow::initTestCase() {
    m_codecs = std::make_unique<dv::CodecRegistry>();
    QCoreApplication::setOrganizationName(QStringLiteral("DicomViewerTests"));
    QCoreApplication::setApplicationName(QStringLiteral("tst_MainWindow"));
}

void TstMainWindow::init() {
    QStandardPaths::setTestModeEnabled(true);
    QSettings().clear();
}

std::unique_ptr<MainWindow> TstMainWindow::showWindow() {
    auto window = std::make_unique<MainWindow>();
    window->show();
    if (!QTest::qWaitForWindowExposed(window.get())) {
        return nullptr;
    }
    return window;
}

void TstMainWindow::startsEmpty() {
    const auto window = showWindow();
    QVERIFY(window != nullptr);
    QCOMPARE(window->windowTitle(), QStringLiteral("DICOM Viewer"));
    QVERIFY(window->viewport()->document() == nullptr);
    for (const QString& text : {QStringLiteral("Ajustar à janela"), QStringLiteral("Tamanho real (100 %)"),
                                QStringLiteral("Ampliar"), QStringLiteral("Reduzir")}) {
        QAction* action = findAction(*window, text);
        QVERIFY2(action != nullptr, qPrintable(text));
        QVERIFY2(!action->isEnabled(), qPrintable(text));
    }
}

void TstMainWindow::openFile_showsImage() {
    const auto window = showWindow();
    QVERIFY(window != nullptr);
    window->openFile(sampleFile("MR_small.dcm"));

    QTRY_VERIFY_WITH_TIMEOUT(window->viewport()->document() != nullptr, kLoadTimeoutMs);
    QCOMPARE(window->viewport()->document()->width, kSmallSide);
    QCOMPARE(window->viewport()->document()->height, kSmallSide);
    QVERIFY(window->windowTitle().startsWith(QStringLiteral("MR_small.dcm")));
    QVERIFY(findAction(*window, QStringLiteral("Ampliar"))->isEnabled());
    QVERIFY(findAction(*window, QStringLiteral("Ajustar à janela"))->isEnabled());
}

void TstMainWindow::openInvalidFile_showsErrorAndKeepsImage() {
    const auto window = showWindow();
    QVERIFY(window != nullptr);
    window->openFile(sampleFile("MR_small.dcm"));
    QTRY_VERIFY_WITH_TIMEOUT(window->viewport()->document() != nullptr, kLoadTimeoutMs);
    const dv::app::DocumentPtr previous = window->viewport()->document();

    window->openFile(sampleFile("not_dicom.dcm"));
    QTRY_VERIFY_WITH_TIMEOUT(visibleMessageBox(*window) != nullptr, kLoadTimeoutMs);
    QMessageBox* box = visibleMessageBox(*window);
    QCOMPARE(box->text(), dv::app::describeLoadError(dv::LoadError{.code = dv::LoadErrorCode::NotDicom}));
    QCOMPARE(window->viewport()->document(), previous);
    box->close();
}

void TstMainWindow::colorImage_disablesWindowControls() {
    const auto window = showWindow();
    QVERIFY(window != nullptr);
    window->openFile(sampleFile("examples_rgb_color.dcm"));
    QTRY_VERIFY_WITH_TIMEOUT(window->viewport()->document() != nullptr, kLoadTimeoutMs);

    auto* controls = window->findChild<dv::app::WindowLevelControls*>();
    QVERIFY(controls != nullptr);
    QVERIFY(!controls->isEnabled());
    QAction* reset = findAction(*window, QStringLiteral("Restaurar janela"));
    QVERIFY(reset != nullptr);
    QVERIFY(!reset->isEnabled());
}

void TstMainWindow::multiFrame_showsNotice() {
    const auto window = showWindow();
    QVERIFY(window != nullptr);
    window->openFile(sampleFile("examples_ybr_color.dcm"));
    QTRY_VERIFY_WITH_TIMEOUT(window->viewport()->document() != nullptr, kLoadTimeoutMs);

    QVERIFY2(window->statusBar()->currentMessage().contains(QStringLiteral("1 de 30")),
             qPrintable(window->statusBar()->currentMessage()));
}

void TstMainWindow::dropFile_opensIt() {
    const auto window = showWindow();
    QVERIFY(window != nullptr);
    QMimeData mimeData;
    mimeData.setUrls({QUrl::fromLocalFile(sampleFile("MR_small.dcm"))});

    QVERIFY(sendDragEnter(*window, mimeData));
    QDropEvent drop(QPointF(window->rect().center()), Qt::CopyAction, &mimeData, Qt::LeftButton,
                    Qt::NoModifier);
    QApplication::sendEvent(window.get(), &drop);
    QTRY_VERIFY_WITH_TIMEOUT(window->viewport()->document() != nullptr, kLoadTimeoutMs);
    QCOMPARE(window->viewport()->document()->width, kSmallSide);
}

void TstMainWindow::dropNonFile_isIgnored() {
    const auto window = showWindow();
    QVERIFY(window != nullptr);
    QMimeData mimeData;
    mimeData.setText(QStringLiteral("not a file"));

    QVERIFY(!sendDragEnter(*window, mimeData));
}

void TstMainWindow::rememberLastDirectory() {
    const QString filePath = sampleFile("MR_small.dcm");
    {
        const auto window = showWindow();
        QVERIFY(window != nullptr);
        window->openFile(filePath);
        QTRY_VERIFY_WITH_TIMEOUT(window->viewport()->document() != nullptr, kLoadTimeoutMs);
    }
    const auto recreated = showWindow();
    QVERIFY(recreated != nullptr);
    QCOMPARE(QSettings().value(QStringLiteral("files/lastDirectory")).toString(),
             QFileInfo(filePath).absolutePath());
}

QTEST_MAIN(TstMainWindow)
#include "tst_MainWindow.moc"
