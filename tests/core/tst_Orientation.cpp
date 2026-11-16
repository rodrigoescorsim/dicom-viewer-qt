#include "core/Orientation.hpp"

#include <QTest>

#include <algorithm>
#include <array>
#include <string>

using dv::EdgeLabels;

class TstOrientation : public QObject {
    Q_OBJECT

private slots:
    void opposite_data();
    void opposite();
    void fromCosines_data();
    void fromCosines();
    void patientOrientation_mammography();
    void patientOrientation_ignoresInvalidChars();
    void imageOrientation_data();
    void imageOrientation();
    void empty();
};

void TstOrientation::opposite_data() {
    QTest::addColumn<QString>("code");
    QTest::addColumn<QString>("expected");

    QTest::newRow("A") << "A" << "P";
    QTest::newRow("P") << "P" << "A";
    QTest::newRow("R") << "R" << "L";
    QTest::newRow("L") << "L" << "R";
    QTest::newRow("H") << "H" << "F";
    QTest::newRow("F") << "F" << "H";
    QTest::newRow("FR") << "FR" << "HL";
    QTest::newRow("invalid") << "x?" << "";
    QTest::newRow("empty") << "" << "";
}

void TstOrientation::opposite() {
    QFETCH(QString, code);
    QFETCH(QString, expected);
    QCOMPARE(dv::oppositeDirection(code.toStdString()), expected.toStdString());
}

void TstOrientation::fromCosines_data() {
    QTest::addColumn<double>("x");
    QTest::addColumn<double>("y");
    QTest::addColumn<double>("z");
    QTest::addColumn<QString>("expected");

    QTest::newRow("+x") << 1.0 << 0.0 << 0.0 << "L";
    QTest::newRow("-x") << -1.0 << 0.0 << 0.0 << "R";
    QTest::newRow("+y") << 0.0 << 1.0 << 0.0 << "P";
    QTest::newRow("-y") << 0.0 << -1.0 << 0.0 << "A";
    QTest::newRow("+z") << 0.0 << 0.0 << 1.0 << "H";
    QTest::newRow("-z") << 0.0 << 0.0 << -1.0 << "F";
    QTest::newRow("oblique_LP") << 0.8 << 0.6 << 0.0 << "LP";
    QTest::newRow("oblique_AL") << 0.6 << -0.8 << 0.0 << "AL";
    QTest::newRow("smallComponentIgnored") << 0.0 << 0.98 << -0.17 << "P";
    QTest::newRow("zero") << 0.0 << 0.0 << 0.0 << "";
}

void TstOrientation::fromCosines() {
    QFETCH(double, x);
    QFETCH(double, y);
    QFETCH(double, z);
    QFETCH(QString, expected);
    QCOMPARE(dv::directionFromCosines(x, y, z), expected.toStdString());
}

void TstOrientation::patientOrientation_mammography() {
    const EdgeLabels labels = dv::edgeLabelsFromPatientOrientation("A", "FR");
    QCOMPARE(labels.left, std::string{"P"});
    QCOMPARE(labels.right, std::string{"A"});
    QCOMPARE(labels.top, std::string{"HL"});
    QCOMPARE(labels.bottom, std::string{"FR"});
}

void TstOrientation::patientOrientation_ignoresInvalidChars() {
    const EdgeLabels labels = dv::edgeLabelsFromPatientOrientation("a?", "F ");
    QCOMPARE(labels.right, std::string{});
    QCOMPARE(labels.left, std::string{});
    QCOMPARE(labels.bottom, std::string{"F"});
    QCOMPARE(labels.top, std::string{"H"});
}

void TstOrientation::imageOrientation_data() {
    QTest::addColumn<QList<double>>("cosines");
    QTest::addColumn<QString>("left");
    QTest::addColumn<QString>("right");
    QTest::addColumn<QString>("top");
    QTest::addColumn<QString>("bottom");

    QTest::newRow("axial") << QList<double>{1, 0, 0, 0, 1, 0} << "R" << "L" << "A" << "P";
    QTest::newRow("coronal") << QList<double>{1, 0, 0, 0, 0, -1} << "R" << "L" << "H" << "F";
    QTest::newRow("sagittal") << QList<double>{0, 1, 0, 0, 0, -1} << "A" << "P" << "H" << "F";
}

void TstOrientation::imageOrientation() {
    QFETCH(QList<double>, cosines);
    QFETCH(QString, left);
    QFETCH(QString, right);
    QFETCH(QString, top);
    QFETCH(QString, bottom);
    QCOMPARE(cosines.size(), 6);

    std::array<double, 6> values{};
    std::ranges::copy(cosines, values.begin());
    const EdgeLabels labels = dv::edgeLabelsFromImageOrientation(values);
    QCOMPARE(labels.left, left.toStdString());
    QCOMPARE(labels.right, right.toStdString());
    QCOMPARE(labels.top, top.toStdString());
    QCOMPARE(labels.bottom, bottom.toStdString());
}

void TstOrientation::empty() {
    QVERIFY(EdgeLabels{}.empty());
    const std::array<double, 6> axial{1, 0, 0, 0, 1, 0};
    QVERIFY(!dv::edgeLabelsFromImageOrientation(axial).empty());
}

QTEST_APPLESS_MAIN(TstOrientation)

#include "tst_Orientation.moc"
