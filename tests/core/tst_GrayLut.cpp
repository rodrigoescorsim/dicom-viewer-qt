#include "core/GrayLut.hpp"

#include <QTest>

#include <array>
#include <cstdint>
#include <vector>

using dv::GrayLutParams;
using dv::IndexRange;
using dv::ValueMapping;
using dv::VoiWindow;

namespace {

constexpr VoiWindow kIdentityWindow{.center = 128.0, .width = 256.0};
constexpr std::uint32_t kByteEntries = 256;
constexpr std::uint8_t kUntouched = 0xEE;

} // namespace

class TstGrayLut : public QObject {
    Q_OBJECT

private slots:
    void identity();
    void inverted();
    void padding_isBlack();
    void mapping_offsetAndScale();
    void padding_beyondTable_isIgnored();
    void padding_partiallyBeyondTable();
    void singleEntry();
    void maxEntries();
    void sanitizesWindow();
    void apply_respectsStride();
    void apply_tightStride();
};

void TstGrayLut::identity() {
    const auto lut = dv::buildGrayLut({.window = kIdentityWindow, .entryCount = kByteEntries});
    QCOMPARE(lut.size(), std::size_t{kByteEntries});
    for (std::uint32_t i = 0; i < kByteEntries; ++i) {
        QCOMPARE(lut[i], static_cast<std::uint8_t>(i));
    }
}

void TstGrayLut::inverted() {
    const auto lut =
        dv::buildGrayLut({.window = kIdentityWindow, .entryCount = kByteEntries, .inverted = true});
    for (std::uint32_t i = 0; i < kByteEntries; ++i) {
        QCOMPARE(lut[i], static_cast<std::uint8_t>(255 - i));
    }
}

void TstGrayLut::padding_isBlack() {
    const auto lut = dv::buildGrayLut({.window = kIdentityWindow,
                                       .entryCount = kByteEntries,
                                       .inverted = true,
                                       .padding = IndexRange{.first = 250, .last = 255}});
    QCOMPARE(lut[249], std::uint8_t{6});
    for (std::uint32_t i = 250; i < kByteEntries; ++i) {
        QCOMPARE(lut[i], std::uint8_t{0});
    }
}

void TstGrayLut::mapping_offsetAndScale() {
    const auto lut = dv::buildGrayLut({.window = kIdentityWindow,
                                       .mapping = ValueMapping{.offset = -100.0, .scale = 2.0},
                                       .entryCount = 200,
                                       .padding = IndexRange{.first = 0, .last = 9}});
    QCOMPARE(lut[0], std::uint8_t{0});
    QCOMPARE(lut[9], std::uint8_t{0});
    QCOMPARE(lut[50], std::uint8_t{0});
    QCOMPARE(lut[51], std::uint8_t{2});
    QCOMPARE(lut[114], std::uint8_t{128});
    QCOMPARE(lut[199], std::uint8_t{255});
}

void TstGrayLut::padding_beyondTable_isIgnored() {
    const auto lut = dv::buildGrayLut(
        {.window = kIdentityWindow, .entryCount = 10, .padding = IndexRange{.first = 20, .last = 30}});
    QCOMPARE(lut.size(), std::size_t{10});
    QCOMPARE(lut[9], std::uint8_t{9});
}

void TstGrayLut::padding_partiallyBeyondTable() {
    const auto lut = dv::buildGrayLut(
        {.window = kIdentityWindow, .entryCount = 10, .padding = IndexRange{.first = 8, .last = 30}});
    QCOMPARE(lut[7], std::uint8_t{7});
    QCOMPARE(lut[8], std::uint8_t{0});
    QCOMPARE(lut[9], std::uint8_t{0});
}

void TstGrayLut::singleEntry() {
    const auto lut = dv::buildGrayLut({.window = kIdentityWindow, .entryCount = 1});
    QCOMPARE(lut.size(), std::size_t{1});
}

void TstGrayLut::maxEntries() {
    constexpr std::uint32_t kMaxEntries = 65536;
    const auto lut = dv::buildGrayLut(
        {.window = VoiWindow{.center = 32768.0, .width = 65536.0}, .entryCount = kMaxEntries});
    QCOMPARE(lut.size(), std::size_t{kMaxEntries});
    QCOMPARE(lut.front(), std::uint8_t{0});
    QCOMPARE(lut.back(), std::uint8_t{255});
}

void TstGrayLut::sanitizesWindow() {
    const auto lut =
        dv::buildGrayLut({.window = VoiWindow{.center = 128.0, .width = 0.0}, .entryCount = kByteEntries});
    for (const std::uint8_t value : lut) {
        QVERIFY(value == 0 || value == 255);
    }
}

void TstGrayLut::apply_respectsStride() {
    const std::vector<std::uint16_t> src{0, 1, 2, 3, 4, 5};
    const std::vector<std::uint8_t> lut{10, 11, 12, 13, 14, 15};
    std::array<std::uint8_t, 10> dst{};
    dst.fill(kUntouched);

    dv::applyGrayLut(src, lut, 3, 2, dst.data(), 5);

    const std::array<std::uint8_t, 10> expected{10, 11, 12, kUntouched, kUntouched,
                                                13, 14, 15, kUntouched, kUntouched};
    QCOMPARE(dst, expected);
}

void TstGrayLut::apply_tightStride() {
    const std::vector<std::uint16_t> src{0, 1, 2, 3, 4, 5};
    const std::vector<std::uint8_t> lut{10, 11, 12, 13, 14, 15};
    std::array<std::uint8_t, 6> dst{};

    dv::applyGrayLut(src, lut, 3, 2, dst.data(), 3);

    const std::array<std::uint8_t, 6> expected{10, 11, 12, 13, 14, 15};
    QCOMPARE(dst, expected);
}

QTEST_APPLESS_MAIN(TstGrayLut)

#include "tst_GrayLut.moc"
