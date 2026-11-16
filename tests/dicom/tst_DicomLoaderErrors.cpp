#include "dicom/CodecRegistry.hpp"
#include "dicom/DicomLoader.hpp"

#include <QTemporaryDir>
#include <QTest>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <random>
#include <string>
#include <string_view>
#include <vector>

#include "support/TestSupport.hpp"

using dv::LoadErrorCode;
using dv::test::samplePath;

namespace {

constexpr std::size_t kRandomFileSize = 4096;
constexpr std::uint32_t kRandomSeed = 20261116;
constexpr std::size_t kPreambleSize = 128;

[[nodiscard]] std::vector<char> readAll(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

void writeAll(const std::filesystem::path& path, const std::vector<char>& bytes) {
    std::ofstream out(path, std::ios::binary);
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

} // namespace

class TstDicomLoaderErrors : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void missingFile();
    void directory();
    void emptyFile();
    void textFile();
    void randomBinary();
    void prefixOnly();
    void structuredReport();
    void truncated();
    void truncatedCopy();
    void jpeg2000();
    void neverThrows();

private:
    [[nodiscard]] std::filesystem::path tempPath(std::string_view fileName) const;
    void expectError(const std::filesystem::path& path, LoadErrorCode code, bool detailRequired = false);

    dv::CodecRegistry m_codecs;
    QTemporaryDir m_tempDir;
};

std::filesystem::path TstDicomLoaderErrors::tempPath(std::string_view fileName) const {
    return std::filesystem::path(m_tempDir.path().toStdU16String()) / fileName;
}

void TstDicomLoaderErrors::expectError(const std::filesystem::path& path, LoadErrorCode code,
                                       bool detailRequired) {
    const auto result = dv::loadDicomFile(path);
    QVERIFY2(!result.has_value(), "the load was expected to fail");
    QCOMPARE(result.error().code, code);
    if (detailRequired) {
        QVERIFY(!result.error().detail.empty());
    }
}

void TstDicomLoaderErrors::initTestCase() {
    QVERIFY(m_tempDir.isValid());

    std::mt19937 generator(kRandomSeed);
    std::uniform_int_distribution<int> byte(0, 255);
    std::vector<char> random(kRandomFileSize);
    for (char& value : random) {
        value = static_cast<char>(byte(generator));
    }
    writeAll(tempPath("random.bin"), random);

    std::vector<char> prefix(kPreambleSize, '\0');
    prefix.insert(prefix.end(), {'D', 'I', 'C', 'M'});
    writeAll(tempPath("prefix_only.dcm"), prefix);

    std::vector<char> mr = readAll(samplePath("MR_small.dcm"));
    QVERIFY(!mr.empty());
    mr.resize(mr.size() / 2);
    writeAll(tempPath("mr_half.dcm"), mr);
}

void TstDicomLoaderErrors::missingFile() {
    expectError(samplePath("nao_existe.dcm"), LoadErrorCode::FileNotFound);
}

void TstDicomLoaderErrors::directory() {
    expectError(std::filesystem::path(DV_SAMPLES_DIR), LoadErrorCode::FileNotReadable);
}

void TstDicomLoaderErrors::emptyFile() {
    expectError(samplePath("empty.dcm"), LoadErrorCode::NotDicom);
}

void TstDicomLoaderErrors::textFile() {
    expectError(samplePath("not_dicom.dcm"), LoadErrorCode::NotDicom, true);
}

void TstDicomLoaderErrors::randomBinary() {
    expectError(tempPath("random.bin"), LoadErrorCode::NotDicom);
}

void TstDicomLoaderErrors::prefixOnly() {
    const auto result = dv::loadDicomFile(tempPath("prefix_only.dcm"));
    QVERIFY(!result.has_value());
    const LoadErrorCode code = result.error().code;
    QVERIFY2(code == LoadErrorCode::NoPixelData || code == LoadErrorCode::CorruptPixelData,
             qPrintable(QString::number(static_cast<int>(code))));
}

void TstDicomLoaderErrors::structuredReport() {
    expectError(samplePath("test-SR.dcm"), LoadErrorCode::NoPixelData);
}

void TstDicomLoaderErrors::truncated() {
    expectError(samplePath("MR_truncated.dcm"), LoadErrorCode::CorruptPixelData, true);
}

void TstDicomLoaderErrors::truncatedCopy() {
    expectError(tempPath("mr_half.dcm"), LoadErrorCode::CorruptPixelData);
}

void TstDicomLoaderErrors::jpeg2000() {
    const auto result = dv::loadDicomFile(samplePath("MR_small_jp2klossless.dcm"));
    QVERIFY(!result.has_value());
    QCOMPARE(result.error().code, LoadErrorCode::UnsupportedTransferSyntax);
    QVERIFY2(result.error().detail.contains("JPEG 2000"), result.error().detail.c_str());
}

void TstDicomLoaderErrors::neverThrows() {
    std::vector<std::filesystem::path> inputs{samplePath("nao_existe.dcm"),
                                              std::filesystem::path(DV_SAMPLES_DIR)};
    for (const auto& entry : std::filesystem::directory_iterator(DV_SAMPLES_DIR)) {
        inputs.push_back(entry.path());
    }
    for (const auto& entry : std::filesystem::directory_iterator(tempPath(""))) {
        inputs.push_back(entry.path());
    }

    for (const auto& path : inputs) {
        try {
            [[maybe_unused]] const auto result = dv::loadDicomFile(path);
        } catch (...) {
            QFAIL(qPrintable(
                QStringLiteral("exception escaped for %1").arg(QString::fromStdU16String(path.u16string()))));
        }
    }
}

QTEST_APPLESS_MAIN(TstDicomLoaderErrors)

#include "tst_DicomLoaderErrors.moc"
