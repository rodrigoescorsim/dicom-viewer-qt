#include "core/Raster.hpp"

#include <QTest>

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <limits>
#include <utility>
#include <vector>

using dv::GrayRaster;
using dv::RgbRaster;

namespace {

[[nodiscard]] GrayRaster grayRaster(int width, int height, std::vector<std::uint16_t> data) {
    return {.width = width, .height = height, .channels = 1, .data = std::move(data)};
}

[[nodiscard]] GrayRaster filledRaster(int width, int height, std::uint16_t value) {
    return grayRaster(width, height,
                      std::vector<std::uint16_t>(
                          static_cast<std::size_t>(width) * static_cast<std::size_t>(height), value));
}

} // namespace

class TstRaster : public QObject {
    Q_OBJECT

private slots:
    void rowAccess();
    void isValid_detectsSizeMismatch();
    void downsample_even();
    void downsample_odd();
    void downsample_noOverflow16();
    void downsample_rgb();
    void downsample_1x1();
    void pyramid_levels();
    void pyramid_small();
    void pyramid_mammography();
    void pyramid_stopsWhenOneSideIsSmall();
    void selectLevel_data();
    void selectLevel();
};

void TstRaster::rowAccess() {
    const GrayRaster raster = grayRaster(3, 2, {1, 2, 3, 4, 5, 6});
    QVERIFY(raster.isValid());
    const auto row = raster.row(1);
    QCOMPARE(std::vector<std::uint16_t>(row.begin(), row.end()), (std::vector<std::uint16_t>{4, 5, 6}));
}

void TstRaster::isValid_detectsSizeMismatch() {
    QVERIFY(!grayRaster(3, 2, {1, 2, 3, 4, 5}).isValid());
}

void TstRaster::downsample_even() {
    const GrayRaster result = dv::downsample2x(grayRaster(4, 2, {0, 2, 10, 20, 4, 6, 30, 41}));
    QCOMPARE(result.width, 2);
    QCOMPARE(result.height, 1);
    QCOMPARE(result.data, (std::vector<std::uint16_t>{3, 25}));
}

void TstRaster::downsample_odd() {
    const GrayRaster result = dv::downsample2x(grayRaster(3, 3, {1, 3, 5, 7, 9, 11, 100, 200, 65535}));
    QCOMPARE(result.width, 2);
    QCOMPARE(result.height, 2);
    QCOMPARE(result.data, (std::vector<std::uint16_t>{5, 8, 150, 65535}));
}

void TstRaster::downsample_noOverflow16() {
    constexpr std::uint16_t kMax = std::numeric_limits<std::uint16_t>::max();
    const GrayRaster result = dv::downsample2x(filledRaster(2, 2, kMax));
    QCOMPARE(result.width, 1);
    QCOMPARE(result.height, 1);
    QCOMPARE(result.data, (std::vector<std::uint16_t>{kMax}));
}

void TstRaster::downsample_rgb() {
    const RgbRaster src{
        .width = 2, .height = 2, .channels = 3, .data = {0, 10, 255, 4, 20, 255, 8, 30, 255, 12, 41, 255}};
    const RgbRaster result = dv::downsample2x(src);
    QCOMPARE(result.width, 1);
    QCOMPARE(result.height, 1);
    QCOMPARE(result.channels, 3);
    QCOMPARE(result.data, (std::vector<std::uint8_t>{6, 25, 255}));
}

void TstRaster::downsample_1x1() {
    const GrayRaster result = dv::downsample2x(grayRaster(1, 1, {42}));
    QCOMPARE(result.width, 1);
    QCOMPARE(result.height, 1);
    QCOMPARE(result.data, (std::vector<std::uint16_t>{42}));
}

void TstRaster::pyramid_levels() {
    constexpr std::uint16_t kValue = 7;
    const auto levels = dv::buildPyramid(filledRaster(1000, 600, kValue));
    QCOMPARE(levels.size(), std::size_t{3});
    const std::pair<int, int> expected[] = {{1000, 600}, {500, 300}, {250, 150}};
    for (std::size_t i = 0; i < levels.size(); ++i) {
        QCOMPARE(levels[i].width, expected[i].first);
        QCOMPARE(levels[i].height, expected[i].second);
        QVERIFY(levels[i].isValid());
        QVERIFY(std::ranges::all_of(levels[i].data, [](std::uint16_t v) { return v == kValue; }));
    }
}

void TstRaster::pyramid_small() {
    QCOMPARE(dv::buildPyramid(filledRaster(4, 2, 0)).size(), std::size_t{1});
}

void TstRaster::pyramid_mammography() {
    const auto levels = dv::buildPyramid(filledRaster(2394, 2850, 0));
    const std::pair<int, int> expected[] = {{2394, 2850}, {1197, 1425}, {599, 713}, {300, 357}, {150, 179}};
    QCOMPARE(levels.size(), std::size(expected));
    for (std::size_t i = 0; i < levels.size(); ++i) {
        QCOMPARE(levels[i].width, expected[i].first);
        QCOMPARE(levels[i].height, expected[i].second);
    }
}

void TstRaster::pyramid_stopsWhenOneSideIsSmall() {
    QCOMPARE(dv::buildPyramid(filledRaster(5000, 200, 0)).size(), std::size_t{1});
}

void TstRaster::selectLevel_data() {
    QTest::addColumn<double>("scale");
    QTest::addColumn<int>("levelCount");
    QTest::addColumn<int>("expected");

    QTest::newRow("1.0") << 1.0 << 5 << 0;
    QTest::newRow("2.0") << 2.0 << 5 << 0;
    QTest::newRow("0.51") << 0.51 << 5 << 0;
    QTest::newRow("0.5") << 0.5 << 5 << 1;
    QTest::newRow("0.26") << 0.26 << 5 << 1;
    QTest::newRow("0.25") << 0.25 << 5 << 2;
    QTest::newRow("0.24") << 0.24 << 5 << 2;
    QTest::newRow("0.001") << 0.001 << 5 << 4;
    QTest::newRow("singleLevel") << 0.1 << 1 << 0;
    QTest::newRow("zero") << 0.0 << 5 << 0;
    QTest::newRow("negative") << -1.0 << 5 << 0;
    QTest::newRow("nan") << std::numeric_limits<double>::quiet_NaN() << 5 << 0;
}

void TstRaster::selectLevel() {
    QFETCH(double, scale);
    QFETCH(int, levelCount);
    QFETCH(int, expected);
    QCOMPARE(dv::selectPyramidLevel(scale, levelCount), expected);
}

QTEST_APPLESS_MAIN(TstRaster)

#include "tst_Raster.moc"
