#include "dicom/CodecRegistry.hpp"
#include "dicom/DicomLoader.hpp"

#include <QTest>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "support/TestSupport.hpp"

using dv::ImageDocument;
using dv::MetadataNode;
using dv::test::samplePath;

namespace {

/// Element (group, element) among `nodes` (one level only), or nullptr.
[[nodiscard]] const MetadataNode* find(std::span<const MetadataNode> nodes, std::uint16_t group,
                                       std::uint16_t element) {
    const auto it = std::ranges::find_if(
        nodes, [&](const MetadataNode& node) { return node.group == group && node.element == element; });
    return it == nodes.end() ? nullptr : &*it;
}

[[nodiscard]] std::size_t countRecursive(std::span<const MetadataNode> nodes) {
    std::size_t count = nodes.size();
    for (const MetadataNode& node : nodes) {
        count += countRecursive(node.children);
    }
    return count;
}

/// Returns the first node, at any depth, that is not an item and has an empty VR; nullptr if none.
[[nodiscard]] const MetadataNode* findUnnamed(std::span<const MetadataNode> nodes) {
    for (const MetadataNode& node : nodes) {
        if (!node.isItem && node.vr.empty()) {
            return &node;
        }
        if (const MetadataNode* child = findUnnamed(node.children)) {
            return child;
        }
    }
    return nullptr;
}

} // namespace

class TstMetadata : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void startsWithFileMeta();
    void counts();
    void sortedByTag();
    void plainValues();
    void multiValue();
    void sequence();
    void binary();
    void binaryLength_uncompressed();
    void utf8Conversion();
    void noEmptyNames();

private:
    [[nodiscard]] const std::vector<MetadataNode>& mammography() const { return m_mammography->metadata; }

    dv::CodecRegistry m_codecs;
    std::optional<ImageDocument> m_mammography;
};

void TstMetadata::initTestCase() {
    auto result = dv::loadDicomFile(samplePath("anonymized_mamo.dcm"));
    QVERIFY(result.has_value());
    m_mammography = std::move(*result);
}

void TstMetadata::startsWithFileMeta() {
    QVERIFY(!mammography().empty());
    QCOMPARE(mammography().front().group, std::uint16_t{0x0002});

    const MetadataNode* syntax = find(mammography(), 0x0002, 0x0010);
    QVERIFY(syntax != nullptr);
    QCOMPARE(syntax->value, std::string{"1.2.840.10008.1.2.4.70"});
    QCOMPARE(syntax->vr, std::string{"UI"});
    QCOMPARE(syntax->name, std::string{"TransferSyntaxUID"});
}

void TstMetadata::counts() {
    QCOMPARE(mammography().size(), std::size_t{167});
    QCOMPARE(countRecursive(mammography()), std::size_t{234});
}

void TstMetadata::sortedByTag() {
    const auto byTag = [](const MetadataNode& a, const MetadataNode& b) {
        return std::pair{a.group, a.element} < std::pair{b.group, b.element};
    };
    QVERIFY(std::ranges::is_sorted(mammography(), byTag));
}

void TstMetadata::plainValues() {
    const MetadataNode* name = find(mammography(), 0x0010, 0x0010);
    QVERIFY(name != nullptr);
    QCOMPARE(name->vr, std::string{"PN"});
    QCOMPARE(name->name, std::string{"PatientName"});
    QCOMPARE(name->value, std::string{"Anonymized^^"});

    const MetadataNode* modality = find(mammography(), 0x0008, 0x0060);
    QVERIFY(modality != nullptr);
    QCOMPARE(modality->vr, std::string{"CS"});
    QCOMPARE(modality->value, std::string{"MG"});
}

void TstMetadata::multiValue() {
    const MetadataNode* center = find(mammography(), 0x0028, 0x1050);
    QVERIFY(center != nullptr);
    QCOMPARE(center->vr, std::string{"DS"});
    QCOMPARE(center->value, std::string{"3091\\3145\\3181"});
}

void TstMetadata::sequence() {
    const MetadataNode* sequence = find(mammography(), 0x0008, 0x2112);
    QVERIFY(sequence != nullptr);
    QCOMPARE(sequence->vr, std::string{"SQ"});
    QVERIFY(sequence->value.empty());
    QCOMPARE(sequence->children.size(), std::size_t{1});

    const MetadataNode& item = sequence->children.front();
    QVERIFY(item.isItem);
    QCOMPARE(item.children.size(), std::size_t{5});
    QCOMPARE(item.children.front().group, std::uint16_t{0x0008});
    QCOMPARE(item.children.front().element, std::uint16_t{0x0000});
}

void TstMetadata::binary() {
    const MetadataNode* privateBinary = find(mammography(), 0x0045, 0x1026);
    QVERIFY(privateBinary != nullptr);
    QVERIFY(privateBinary->isBinary);
    QCOMPARE(privateBinary->byteLength, std::uint64_t{1024});
    QVERIFY(privateBinary->value.empty());

    const MetadataNode* pixelData = find(mammography(), 0x7fe0, 0x0010);
    QVERIFY(pixelData != nullptr);
    QVERIFY(pixelData->isBinary);
    QVERIFY(pixelData->value.empty());
}

void TstMetadata::binaryLength_uncompressed() {
    const auto result = dv::loadDicomFile(samplePath("MR_small.dcm"));
    QVERIFY(result.has_value());
    const MetadataNode* pixelData = find(result->metadata, 0x7fe0, 0x0010);
    QVERIFY(pixelData != nullptr);
    QCOMPARE(pixelData->byteLength, std::uint64_t{8192});
}

void TstMetadata::utf8Conversion() {
    const auto result = dv::loadDicomFile(samplePath("synthetic_nonsquare_latin1.dcm"));
    QVERIFY(result.has_value());
    const MetadataNode* name = find(result->metadata, 0x0010, 0x0010);
    QVERIFY(name != nullptr);
    // "Conceição^João" spelled as UTF-8 bytes, independent of the source/execution character sets.
    QCOMPARE(name->value, std::string{"Concei\xC3\xA7\xC3\xA3o^Jo\xC3\xA3o"});
}

void TstMetadata::noEmptyNames() {
    int loaded = 0;
    for (const auto& entry : std::filesystem::directory_iterator(DV_SAMPLES_DIR)) {
        if (entry.path().extension() != ".dcm") {
            continue;
        }
        const auto result = dv::loadDicomFile(entry.path());
        if (!result) {
            continue;
        }
        ++loaded;
        const MetadataNode* unnamed = findUnnamed(result->metadata);
        QVERIFY2(unnamed == nullptr,
                 qPrintable(QString::fromStdU16String(entry.path().filename().u16string())));
    }
    QVERIFY(loaded > 0);
}

QTEST_APPLESS_MAIN(TstMetadata)

#include "tst_Metadata.moc"
