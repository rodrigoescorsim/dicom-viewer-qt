#include "app/MainWindow.hpp"
#include "dicom/CodecRegistry.hpp"

#include <QApplication>
#include <QCommandLineParser>
#include <QStringList>
#include <QStyleFactory>
#include <QStyleHints>

int main(int argc, char* argv[]) {
    const dv::CodecRegistry codecs; // declared first: destroyed last, after every worker has finished

    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("DicomViewer"));
    QCoreApplication::setApplicationName(QStringLiteral("DICOM Viewer"));
    QCoreApplication::setApplicationVersion(QStringLiteral(DV_VERSION));
    QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Dark);

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
