#include <QTest>

class TstSmoke : public QObject {
    Q_OBJECT

private slots:
    void buildRuns() { QVERIFY(true); }
};

QTEST_APPLESS_MAIN(TstSmoke)

#include "tst_Smoke.moc"
