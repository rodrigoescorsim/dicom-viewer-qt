#include "dicom/CodecRegistry.hpp"
#include "dicom/DicomLoader.hpp"

#include <QTemporaryDir>
#include <QTest>

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#include "support/TestSupport.hpp"

using dv::ImageDocument;
using dv::IndexRange;
using dv::VoiFunction;
using dv::test::monoValueAt;
using dv::test::rgbAt;
using dv::test::samplePath;

namespace {

struct Position {
    int column{0};
    int row{0};
};

/// Sample points (column, row): p00 = (0, 0), pc = (W/2, H/2), pl = (W-1, H-1), pq = (W/4, H/3).
[[nodiscard]] std::array<Position, 4> referencePositions(int width, int height) {
    return {{{0, 0}, {width / 2, height / 2}, {width - 1, height - 1}, {width / 4, height / 3}}};
}

[[nodiscard]] ImageDocument loadOrFail(const std::string& fileName) {
    auto result = dv::loadDicomFile(samplePath(fileName));
    if (!result) {
        qFatal("Could not load %s: code %d, %s", fileName.c_str(), static_cast<int>(result.error().code),
               result.error().detail.c_str());
    }
    return std::move(*result);
}

/// Level 0 rendered with the default window, as the viewer does it.
[[nodiscard]] std::vector<std::uint8_t> renderDefault(const ImageDocument& document) {
    const dv::MonoImage& mono = *document.mono();
    const auto lut = dv::buildGrayLut({.window = mono.defaultWindow,
                                       .mapping = mono.mapping,
                                       .entryCount = mono.lutEntryCount,
                                       .inverted = mono.inverted,
                                       .padding = mono.padding});
    const dv::GrayRaster& level0 = mono.levels.front();
    std::vector<std::uint8_t> gray(level0.data.size());
    dv::applyGrayLut(level0.data, lut, level0.width, level0.height, gray.data(), level0.width);
    return gray;
}

[[nodiscard]] std::uint64_t sumOf(const std::vector<std::uint8_t>& gray) {
    std::uint64_t sum = 0;
    for (const std::uint8_t value : gray) {
        sum += value;
    }
    return sum;
}

[[nodiscard]] int grayAt(const std::vector<std::uint8_t>& gray, int width, Position p) {
    return gray[static_cast<std::size_t>(p.row) * static_cast<std::size_t>(width) +
                static_cast<std::size_t>(p.column)];
}

} // namespace

class TstDicomLoader : public QObject {
    Q_OBJECT

private slots:
    void monochrome_data();
    void monochrome();
    void mammography_details();
    void mammography_rendersLikeReference();
    void mr_rendersLikeReference_data();
    void mr_rendersLikeReference();
    void ct_appliesRescale();
    void overlay_presetLabels();
    void dose_quantizesWideRange();
    void color_data();
    void color();
    void multiFrame_loadsFirstFrame();
    void unicodePath();
    void toolkitVersion();
    void loadIsRepeatable();

private:
    dv::CodecRegistry m_codecs;
};

void TstDicomLoader::monochrome_data() {
    QTest::addColumn<QString>("file");
    QTest::addColumn<int>("width");
    QTest::addColumn<int>("height");
    QTest::addColumn<double>("minValue");
    QTest::addColumn<double>("maxValue");
    QTest::addColumn<double>("minIncludingPadding");
    QTest::addColumn<QList<double>>("pixels"); // p00, pc, pl, pq; empty for lossy files
    QTest::addColumn<bool>("inverted");
    QTest::addColumn<double>("aspect");
    QTest::addColumn<int>("presetCount");
    QTest::addColumn<double>("center");
    QTest::addColumn<double>("windowWidth");
    QTest::addColumn<int>("function");

    const auto linear = static_cast<int>(VoiFunction::Linear);
    const auto sigmoid = static_cast<int>(VoiFunction::Sigmoid);
    const QList<double> mrPixels{905, 182, 862, 467};

    QTest::newRow("mammography") << "anonymized_mamo.dcm" << 2394 << 2850 << 1061.0 << 3541.0 << 847.0
                                 << QList<double>{3446, 947, 2554, 2801} << false << 1.0 << 3 << 3091.0
                                 << 1150.0 << sigmoid;
    QTest::newRow("ct") << "CT_small.dcm" << 128 << 128 << -896.0 << 1167.0 << -896.0
                        << QList<double>{-849, 904, -115, 112} << false << 1.0 << 0 << 136.0 << 2064.0
                        << linear;

    const char* const mrFiles[] = {"MR_small.dcm", "MR_small_implicit.dcm", "MR_small_bigendian.dcm",
                                   "MR_small_RLE.dcm", "MR_small_jpeg_ls_lossless.dcm"};
    for (const char* file : mrFiles) {
        QTest::newRow(file) << file << 64 << 64 << 127.0 << 2145.0 << 127.0 << mrPixels << false << 1.0 << 1
                            << 600.0 << 1600.0 << linear;
    }
    QTest::newRow("monochrome1") << "synthetic_monochrome1.dcm" << 64 << 64 << 127.0 << 2145.0 << 127.0
                                 << mrPixels << true << 1.0 << 1 << 600.0 << 1600.0 << linear;
    QTest::newRow("nonsquare") << "synthetic_nonsquare_latin1.dcm" << 64 << 64 << 127.0 << 2145.0 << 127.0
                               << mrPixels << false << 2.0 << 1 << 600.0 << 1600.0 << linear;
    QTest::newRow("overlay") << "examples_overlay.dcm" << 484 << 300 << 0.0 << 1123.0 << 0.0
                             << QList<double>{0, 136, 6, 420} << false << 1.0 << 2 << 450.0 << 790.0
                             << linear;
    QTest::newRow("jpegExtended") << "JPGExtended.dcm" << 256 << 1024 << 0.0 << 264.0 << 0.0
                                  << QList<double>{} << false << 1.0 << 0 << 132.5 << 265.0 << linear;
    QTest::newRow("deflated") << "image_dfl.dcm" << 512 << 512 << 0.0 << 255.0 << 0.0
                              << QList<double>{213, 65, 188, 41} << false << 1.0 << 0 << 128.0 << 256.0
                              << linear;
}

void TstDicomLoader::monochrome() {
    QFETCH(QString, file);
    QFETCH(int, width);
    QFETCH(int, height);
    QFETCH(double, minValue);
    QFETCH(double, maxValue);
    QFETCH(double, minIncludingPadding);
    QFETCH(QList<double>, pixels);
    QFETCH(bool, inverted);
    QFETCH(double, aspect);
    QFETCH(int, presetCount);
    QFETCH(double, center);
    QFETCH(double, windowWidth);
    QFETCH(int, function);

    const ImageDocument document = loadOrFail(file.toStdString());
    const dv::MonoImage* mono = document.mono();
    QVERIFY(mono != nullptr);
    QCOMPARE(document.width, width);
    QCOMPARE(document.height, height);
    QVERIFY(!mono->levels.empty());
    QVERIFY(mono->levels.front().isValid());
    QCOMPARE(mono->levels.front().width, width);
    QCOMPARE(mono->levels.front().height, height);

    QCOMPARE(mono->mapping.scale, 1.0);
    QCOMPARE(mono->minValue, minValue);
    QCOMPARE(mono->maxValue, maxValue);
    QCOMPARE(mono->lutEntryCount, static_cast<std::uint32_t>(maxValue - minIncludingPadding + 1.0));
    QCOMPARE(mono->inverted, inverted);
    QCOMPARE(document.pixelAspectRatio, aspect);
    QCOMPARE(static_cast<int>(mono->presets.size()), presetCount);
    QCOMPARE(mono->defaultWindow.center, center);
    QCOMPARE(mono->defaultWindow.width, windowWidth);
    QCOMPARE(mono->defaultWindow.function, static_cast<VoiFunction>(function));

    const auto positions = referencePositions(width, height);
    for (qsizetype i = 0; i < pixels.size(); ++i) {
        const Position p = positions[static_cast<std::size_t>(i)];
        QCOMPARE(monoValueAt(document, p.column, p.row), pixels[i]);
    }
}

void TstDicomLoader::mammography_details() {
    const ImageDocument document = loadOrFail("anonymized_mamo.dcm");
    const dv::MonoImage& mono = *document.mono();

    QCOMPARE(mono.mapping.offset, 847.0);
    QCOMPARE(mono.lutEntryCount, std::uint32_t{2695});
    QVERIFY(mono.padding.has_value());
    QCOMPARE(*mono.padding, (IndexRange{.first = 0, .last = 213}));

    const std::array<std::array<int, 2>, 5> levelSizes{
        {{2394, 2850}, {1197, 1425}, {599, 713}, {300, 357}, {150, 179}}};
    QCOMPARE(mono.levels.size(), levelSizes.size());
    for (std::size_t i = 0; i < levelSizes.size(); ++i) {
        QCOMPARE(mono.levels[i].width, levelSizes[i][0]);
        QCOMPARE(mono.levels[i].height, levelSizes[i][1]);
    }

    struct ExpectedPreset {
        const char* label;
        double center;
        double width;
    };
    const std::array<ExpectedPreset, 3> presets{
        {{"SOFTER", 3091.0, 1150.0}, {"NORMAL", 3145.0, 1000.0}, {"HARDER", 3181.0, 850.0}}};
    QCOMPARE(mono.presets.size(), presets.size());
    for (std::size_t i = 0; i < presets.size(); ++i) {
        QCOMPARE(mono.presets[i].label, std::string{presets[i].label});
        QCOMPARE(mono.presets[i].window.center, presets[i].center);
        QCOMPARE(mono.presets[i].window.width, presets[i].width);
        QCOMPARE(mono.presets[i].window.function, VoiFunction::Sigmoid);
    }

    QCOMPARE(document.orientation.left, std::string{"P"});
    QCOMPARE(document.orientation.right, std::string{"A"});
    QCOMPARE(document.orientation.top, std::string{"HL"});
    QCOMPARE(document.orientation.bottom, std::string{"FR"});

    QCOMPARE(document.info.modality, std::string{"MG"});
    QCOMPARE(document.info.bitsAllocated, 16);
    QCOMPARE(document.info.bitsStored, 12);
    QCOMPARE(document.info.photometricInterpretation, std::string{"MONOCHROME2"});
    QVERIFY2(document.info.transferSyntaxName.contains("JPEG Lossless"),
             document.info.transferSyntaxName.c_str());
    QCOMPARE(document.info.frameCount, 1);
}

void TstDicomLoader::mammography_rendersLikeReference() {
    const ImageDocument document = loadOrFail("anonymized_mamo.dcm");
    const auto gray = renderDefault(document);
    QCOMPARE(sumOf(gray), std::uint64_t{108444971});

    const auto positions = referencePositions(document.width, document.height);
    const std::array<int, 4> expected{198, 0, 34, 68};
    for (std::size_t i = 0; i < expected.size(); ++i) {
        QCOMPARE(grayAt(gray, document.width, positions[i]), expected[i]);
    }
}

void TstDicomLoader::mr_rendersLikeReference_data() {
    QTest::addColumn<QString>("file");
    QTest::addColumn<qulonglong>("sum");
    QTest::addColumn<QList<int>>("expected");

    QTest::newRow("mr") << "MR_small.dcm" << qulonglong{463120} << QList<int>{176, 61, 169, 106};
    QTest::newRow("monochrome1") << "synthetic_monochrome1.dcm" << qulonglong{581360}
                                 << QList<int>{79, 194, 86, 149};
}

void TstDicomLoader::mr_rendersLikeReference() {
    QFETCH(QString, file);
    QFETCH(qulonglong, sum);
    QFETCH(QList<int>, expected);

    const ImageDocument document = loadOrFail(file.toStdString());
    const auto gray = renderDefault(document);
    QCOMPARE(sumOf(gray), std::uint64_t{sum});

    const auto positions = referencePositions(document.width, document.height);
    for (qsizetype i = 0; i < expected.size(); ++i) {
        QCOMPARE(grayAt(gray, document.width, positions[static_cast<std::size_t>(i)]), expected[i]);
    }
}

void TstDicomLoader::ct_appliesRescale() {
    const ImageDocument document = loadOrFail("CT_small.dcm");
    QCOMPARE(document.mono()->mapping.offset, -896.0);
    QCOMPARE(document.orientation.left, std::string{"R"});
    QCOMPARE(document.orientation.right, std::string{"L"});
    QCOMPARE(document.orientation.top, std::string{"A"});
    QCOMPARE(document.orientation.bottom, std::string{"P"});
}

void TstDicomLoader::overlay_presetLabels() {
    const ImageDocument document = loadOrFail("examples_overlay.dcm");
    const dv::MonoImage& mono = *document.mono();

    QCOMPARE(mono.presets.size(), std::size_t{2});
    QCOMPARE(mono.presets[0].label, std::string{"WINDOW1"});
    QCOMPARE(mono.presets[0].window.center, 450.0);
    QCOMPARE(mono.presets[0].window.width, 790.0);
    QCOMPARE(mono.presets[1].label, std::string{"WINDOW2"});
    QCOMPARE(mono.presets[1].window.center, 200.0);
    QCOMPARE(mono.presets[1].window.width, 443.0);

    QCOMPARE(mono.levels.size(), std::size_t{2});
    QCOMPARE(mono.levels[0].width, 484);
    QCOMPARE(mono.levels[0].height, 300);
    QCOMPARE(mono.levels[1].width, 242);
    QCOMPARE(mono.levels[1].height, 150);
}

void TstDicomLoader::dose_quantizesWideRange() {
    constexpr double kScaleTolerance = 1e-5;
    constexpr double kValueTolerance = 4.0;

    const ImageDocument document = loadOrFail("rtdose_1frame.dcm");
    const dv::MonoImage& mono = *document.mono();

    QCOMPARE(document.width, 10);
    QCOMPARE(document.height, 10);
    QCOMPARE(mono.mapping.offset, 795000.0);
    QVERIFY2(std::abs(mono.mapping.scale - 7.003891) < kScaleTolerance,
             qPrintable(QString::number(mono.mapping.scale)));
    QCOMPARE(mono.lutEntryCount, std::uint32_t{65536});
    QCOMPARE(mono.minValue, 795000.0);
    QCOMPARE(mono.maxValue, 1254000.0);

    const double p00 = monoValueAt(document, 0, 0);
    QVERIFY2(std::abs(p00 - 1249000.0) <= kValueTolerance, qPrintable(QString::number(p00, 'f', 3)));
    QCOMPARE(mono.defaultWindow.center, 1024500.5);
    QCOMPARE(mono.defaultWindow.width, 459001.0);
}

void TstDicomLoader::color_data() {
    QTest::addColumn<QString>("file");
    QTest::addColumn<int>("width");
    QTest::addColumn<int>("height");
    QTest::addColumn<QList<int>>("expected"); // p00, pc, pl as R, G, B
    QTest::addColumn<int>("tolerance");
    QTest::addColumn<int>("levelCount"); // 0 = not checked

    QTest::newRow("rgb") << "examples_rgb_color.dcm" << 320 << 240 << QList<int>{0, 0, 0, 10, 10, 10, 0, 0, 0}
                         << 0 << 0;
    QTest::newRow("rgbOdd") << "SC_rgb_small_odd.dcm" << 3 << 3
                            << QList<int>{166, 141, 52, 63, 87, 176, 158, 158, 158} << 0 << 0;
    QTest::newRow("rgbPlanarBigEndian")
        << "ExplVR_BigEnd.dcm" << 80 << 60 << QList<int>{171, 171, 171, 255, 255, 0, 255, 232, 0} << 0 << 0;
    QTest::newRow("rgbJpegLossless") << "SC_rgb_jpeg_gdcm.dcm" << 100 << 100
                                     << QList<int>{255, 0, 0, 128, 128, 255, 255, 255, 255} << 0 << 0;
    QTest::newRow("ybrFull422") << "SC_ybr_full_422_uncompressed.dcm" << 100 << 100
                                << QList<int>{254, 0, 0, 125, 129, 255, 255, 254, 255} << 2 << 0;
    QTest::newRow("ybr422Jpeg") << "SC_ybr422_jpeg.dcm" << 100 << 100
                                << QList<int>{254, 0, 0, 128, 124, 255, 255, 255, 255} << 3 << 0;
    QTest::newRow("palette") << "examples_palette.dcm" << 800 << 350
                             << QList<int>{37, 62, 94, 1, 1, 1, 0, 0, 0} << 0 << 2;
}

void TstDicomLoader::color() {
    QFETCH(QString, file);
    QFETCH(int, width);
    QFETCH(int, height);
    QFETCH(QList<int>, expected);
    QFETCH(int, tolerance);
    QFETCH(int, levelCount);

    const ImageDocument document = loadOrFail(file.toStdString());
    const dv::ColorImage* color = document.color();
    QVERIFY(color != nullptr);
    QCOMPARE(document.width, width);
    QCOMPARE(document.height, height);
    const dv::RgbRaster& level0 = color->levels.front();
    QCOMPARE(level0.channels, 3);
    QCOMPARE(level0.data.size(), static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 3U);
    if (levelCount > 0) {
        QCOMPARE(document.levelCount(), levelCount);
    }

    const auto positions = referencePositions(width, height);
    for (std::size_t p = 0; p < 3; ++p) {
        const auto rgb = rgbAt(document, positions[p].column, positions[p].row);
        for (std::size_t c = 0; c < 3; ++c) {
            const int want = expected[static_cast<qsizetype>(p * 3 + c)];
            QVERIFY2(std::abs(rgb[c] - want) <= tolerance,
                     qPrintable(QStringLiteral("position %1 channel %2: expected %3, got %4")
                                    .arg(p)
                                    .arg(c)
                                    .arg(want)
                                    .arg(rgb[c])));
        }
    }
}

void TstDicomLoader::multiFrame_loadsFirstFrame() {
    const ImageDocument document = loadOrFail("examples_ybr_color.dcm");
    QVERIFY(document.color() != nullptr);
    QCOMPARE(document.width, 320);
    QCOMPARE(document.height, 240);
    QCOMPARE(document.info.frameCount, 30);
}

void TstDicomLoader::unicodePath() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const std::filesystem::path target =
        std::filesystem::path(dir.path().toStdU16String()) / std::filesystem::path(u8"imagem_ção_日本.dcm");
    QVERIFY(std::filesystem::copy_file(samplePath("MR_small.dcm"), target));

    const auto result = dv::loadDicomFile(target);
    QVERIFY(result.has_value());
    QCOMPARE(result->width, 64);
    QCOMPARE(result->height, 64);
}

void TstDicomLoader::toolkitVersion() {
    QVERIFY(dv::dicomToolkitVersion().starts_with("3."));
}

void TstDicomLoader::loadIsRepeatable() {
    const auto first = sumOf(renderDefault(loadOrFail("anonymized_mamo.dcm")));
    const auto second = sumOf(renderDefault(loadOrFail("anonymized_mamo.dcm")));
    QCOMPARE(first, second);
    QCOMPARE(first, std::uint64_t{108444971});
}

QTEST_APPLESS_MAIN(TstDicomLoader)

#include "tst_DicomLoader.moc"
