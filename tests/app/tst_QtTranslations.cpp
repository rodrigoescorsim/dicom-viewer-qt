#include "app/QtTranslations.hpp"

#include <QCoreApplication>
#include <QTest>

/// Qt's built-in texts (e.g. the "Show Details..." button of the error dialog) must not stay in English.
class TstQtTranslations : public QObject {
    Q_OBJECT

private slots:
    void translatesBuiltInDialogTexts();
};

void TstQtTranslations::translatesBuiltInDialogTexts() {
    const QString english = QStringLiteral("Show Details...");
    QCOMPARE(QCoreApplication::translate("QMessageBox", "Show Details..."), english);

    QVERIFY(dv::app::installQtTranslations(*QCoreApplication::instance()));
    const QString translated = QCoreApplication::translate("QMessageBox", "Show Details...");
    QVERIFY2(translated != english, qPrintable(translated));
    QVERIFY2(translated.contains(QStringLiteral("detalhes"), Qt::CaseInsensitive), qPrintable(translated));
}

QTEST_GUILESS_MAIN(TstQtTranslations)
#include "tst_QtTranslations.moc"
