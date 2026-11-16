#include "core/Voi.hpp"

#include <QTest>

#include <cmath>
#include <limits>

using dv::VoiFunction;
using dv::VoiWindow;

namespace {

[[nodiscard]] long gray(const VoiWindow& window, double x) {
    return std::lround(dv::evaluateVoi(window, x) * 255.0);
}

} // namespace

class TstVoi : public QObject {
    Q_OBJECT

private slots:
    void evaluate_data();
    void evaluate();
    void evaluate_isMonotonic_data();
    void evaluate_isMonotonic();
    void sanitized_fixesInvalidWidth_data();
    void sanitized_fixesInvalidWidth();
    void sanitized_fixesInvalidCenter();
    void sanitized_keepsValid();
    void fullRange_data();
    void fullRange();
    void fullRange_mapsExtremes_data();
    void fullRange_mapsExtremes();
};

void TstVoi::evaluate_data() {
    QTest::addColumn<double>("center");
    QTest::addColumn<double>("width");
    QTest::addColumn<int>("function");
    QTest::addColumn<QList<double>>("inputs");
    QTest::addColumn<QList<int>>("expected");

    const auto linear = static_cast<int>(VoiFunction::Linear);
    const auto linearExact = static_cast<int>(VoiFunction::LinearExact);
    const auto sigmoid = static_cast<int>(VoiFunction::Sigmoid);

    QTest::newRow("linear_mr") << 600.0 << 1600.0 << linear
                               << QList<double>{-200, -199, 127, 182, 599.5, 905, 1399, 1400, 2145}
                               << QList<int>{0, 0, 52, 61, 128, 176, 255, 255, 255};
    QTest::newRow("linear_identity") << 128.0 << 256.0 << linear << QList<double>{0, 1, 127, 128, 254, 255}
                                     << QList<int>{0, 1, 127, 128, 254, 255};
    QTest::newRow("linear_width1") << 10.0 << 1.0 << linear << QList<double>{9, 9.5, 9.6, 10}
                                   << QList<int>{0, 0, 255, 255};
    QTest::newRow("linearExact") << 100.0 << 200.0 << linearExact
                                 << QList<double>{0, 1, 50, 100, 150, 200, 201}
                                 << QList<int>{0, 1, 64, 128, 191, 255, 255};
    QTest::newRow("sigmoid_softer") << 3091.0 << 1150.0 << sigmoid
                                    << QList<double>{847, 1060, 2554, 2801, 3091, 3446, 3541, 4095}
                                    << QList<int>{0, 0, 34, 68, 128, 198, 211, 247};
    QTest::newRow("sigmoid_normal") << 3145.0 << 1000.0 << sigmoid
                                    << QList<double>{947, 2554, 2801, 3145, 3446, 3541}
                                    << QList<int>{0, 22, 51, 128, 196, 212};
    QTest::newRow("sigmoid_harder") << 3181.0 << 850.0 << sigmoid
                                    << QList<double>{947, 2554, 2801, 3181, 3446, 3541}
                                    << QList<int>{0, 13, 37, 128, 198, 215};
}

void TstVoi::evaluate() {
    QFETCH(double, center);
    QFETCH(double, width);
    QFETCH(int, function);
    QFETCH(QList<double>, inputs);
    QFETCH(QList<int>, expected);
    QCOMPARE(inputs.size(), expected.size());

    const VoiWindow window{.center = center, .width = width, .function = static_cast<VoiFunction>(function)};
    for (qsizetype i = 0; i < inputs.size(); ++i) {
        const long actual = gray(window, inputs[i]);
        if (actual != expected[i]) {
            QFAIL(qPrintable(
                QStringLiteral("x = %1: expected %2, got %3").arg(inputs[i]).arg(expected[i]).arg(actual)));
        }
    }
}

void TstVoi::evaluate_isMonotonic_data() {
    QTest::addColumn<int>("function");
    QTest::newRow("linear") << static_cast<int>(VoiFunction::Linear);
    QTest::newRow("linearExact") << static_cast<int>(VoiFunction::LinearExact);
    QTest::newRow("sigmoid") << static_cast<int>(VoiFunction::Sigmoid);
}

void TstVoi::evaluate_isMonotonic() {
    QFETCH(int, function);
    constexpr int kLastInput = 2000;
    const VoiWindow window{.center = 1000.0, .width = 500.0, .function = static_cast<VoiFunction>(function)};

    double previous = dv::evaluateVoi(window, 0.0);
    for (int x = 0; x <= kLastInput; ++x) {
        const double y = dv::evaluateVoi(window, x);
        QVERIFY2(y >= 0.0 && y <= 1.0, qPrintable(QString::number(x)));
        QVERIFY2(y >= previous, qPrintable(QString::number(x)));
        previous = y;
    }
}

void TstVoi::sanitized_fixesInvalidWidth_data() {
    QTest::addColumn<double>("width");
    QTest::newRow("zero") << 0.0;
    QTest::newRow("negative") << -5.0;
    QTest::newRow("nan") << std::numeric_limits<double>::quiet_NaN();
    QTest::newRow("infinity") << std::numeric_limits<double>::infinity();
}

void TstVoi::sanitized_fixesInvalidWidth() {
    QFETCH(double, width);
    const VoiWindow result = dv::sanitized({.center = 1.0, .width = width});
    QCOMPARE(result.width, dv::kMinWindowWidth);
    QCOMPARE(result.center, 1.0);
}

void TstVoi::sanitized_fixesInvalidCenter() {
    const VoiWindow result = dv::sanitized({.center = std::numeric_limits<double>::quiet_NaN(),
                                            .width = 10.0,
                                            .function = VoiFunction::Sigmoid});
    QCOMPARE(result.center, 0.0);
    QCOMPARE(result.width, 10.0);
    QCOMPARE(result.function, VoiFunction::Sigmoid);
}

void TstVoi::sanitized_keepsValid() {
    const VoiWindow window{.center = 3091.0, .width = 1150.0, .function = VoiFunction::Sigmoid};
    QCOMPARE(dv::sanitized(window), window);
}

void TstVoi::fullRange_data() {
    QTest::addColumn<double>("minValue");
    QTest::addColumn<double>("maxValue");
    QTest::addColumn<double>("center");
    QTest::addColumn<double>("width");

    QTest::newRow("ct") << -896.0 << 1167.0 << 136.0 << 2064.0;
    QTest::newRow("mr") << 0.0 << 264.0 << 132.5 << 265.0;
    QTest::newRow("constant") << 5.0 << 5.0 << 5.5 << 1.0;
    QTest::newRow("reversed") << 10.0 << 0.0 << 5.5 << 11.0;
}

void TstVoi::fullRange() {
    QFETCH(double, minValue);
    QFETCH(double, maxValue);
    QFETCH(double, center);
    QFETCH(double, width);

    const VoiWindow window = dv::fullRangeWindow(minValue, maxValue);
    QCOMPARE(window.center, center);
    QCOMPARE(window.width, width);
    QCOMPARE(window.function, VoiFunction::Linear);
}

void TstVoi::fullRange_mapsExtremes_data() {
    QTest::addColumn<double>("minValue");
    QTest::addColumn<double>("maxValue");
    QTest::newRow("ct") << -896.0 << 1167.0;
    QTest::newRow("12bit") << 0.0 << 4095.0;
}

void TstVoi::fullRange_mapsExtremes() {
    QFETCH(double, minValue);
    QFETCH(double, maxValue);
    const VoiWindow window = dv::fullRangeWindow(minValue, maxValue);
    QCOMPARE(gray(window, minValue), 0L);
    QCOMPARE(gray(window, maxValue), 255L);
}

QTEST_APPLESS_MAIN(TstVoi)

#include "tst_Voi.moc"
