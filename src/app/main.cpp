#include "app/MainWindow.hpp"
#include "app/QtTranslations.hpp"
#include "dicom/CodecRegistry.hpp"

#include <QApplication>
#include <QCommandLineParser>
#include <QPalette>
#include <QStringList>
#include <QStyleFactory>
#include <QStyleHints>

int main(int argc, char* argv[]) {
    const dv::CodecRegistry codecs; // declared first: destroyed last, after every worker has finished

    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("DicomViewer"));
    QCoreApplication::setApplicationName(QStringLiteral("DICOM Viewer"));
    QCoreApplication::setApplicationVersion(QStringLiteral(DV_VERSION));
    // Optional: without the catalog a few built-in Qt texts stay in English.
    static_cast<void>(dv::app::installQtTranslations(app));
    QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Dark);
    {
        // The dark palette paints alternate rows in a saturated blue; use a neutral shade instead.
        constexpr int kAlternateRowLightness = 125; // percent of Base
        QPalette palette = QApplication::palette();
        palette.setColor(QPalette::AlternateBase,
                         palette.color(QPalette::Base).lighter(kAlternateRowLightness));
        QApplication::setPalette(palette);
    }

    QCommandLineParser parser;
    parser.setApplicationDescription(QCoreApplication::translate("main", "Visualizador de imagens DICOM"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(QStringLiteral("arquivo"),
                                 QCoreApplication::translate("main", "Arquivo DICOM a abrir."),
                                 QStringLiteral("[arquivo]"));
    parser.process(app);

    dv::app::MainWindow window;
    window.show();
    if (const QStringList files = parser.positionalArguments(); !files.isEmpty()) {
        window.openFile(files.constFirst());
    }
    return QApplication::exec();
}
