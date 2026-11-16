#include <QApplication>
#include <QMainWindow>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    QMainWindow window;
    window.setWindowTitle(QStringLiteral("DICOM Viewer"));
    window.resize(1024, 768);
    window.show();

    return QApplication::exec();
}
