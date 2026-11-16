#include "app/MetadataPanel.hpp"
#include "core/ImageDocument.hpp"

#include <QAbstractItemModel>
#include <QLineEdit>
#include <QLocale>
#include <QTest>

#include <memory>
#include <string>
#include <vector>

using dv::MetadataNode;
using dv::app::MetadataPanel;

namespace {

constexpr int kTag = 0;
constexpr int kName = 1;
constexpr int kVr = 2;
constexpr int kValue = 3;
constexpr int kRootRows = 5;
constexpr int kSequenceRow = 2;
constexpr int kPixelDataRow = 3;
constexpr int kLongValueRow = 4;
constexpr qsizetype kLongValueLength = 500;
constexpr qsizetype kDisplayedLength = 201; // 200 characters + "…"

[[nodiscard]] MetadataNode element(std::uint16_t group, std::uint16_t number, std::string name,
                                   std::string vr, std::string value) {
    return {.group = group,
            .element = number,
            .name = std::move(name),
            .vr = std::move(vr),
            .value = std::move(value)};
}

[[nodiscard]] MetadataNode sequenceItem() {
    return {.children = {element(0x0008, 0x1150, "ReferencedSOPClassUID", "UI", "1.2.3")}, .isItem = true};
}

[[nodiscard]] std::vector<MetadataNode> syntheticTree() {
    MetadataNode sequence = element(0x0008, 0x2112, "SourceImageSequence", "SQ", "");
    sequence.children = {sequenceItem(), sequenceItem()};
    MetadataNode pixelData = element(0x7fe0, 0x0010, "PixelData", "OW", "");
    pixelData.isBinary = true;
    pixelData.byteLength = 1024;
    return {
        element(0x0008, 0x0060, "Modality", "CS", "MG"),
        element(0x0010, 0x0010, "PatientName", "PN", "Conceição^João"),
        std::move(sequence),
        std::move(pixelData),
        element(0x0020, 0x4000, "ImageComments", "LT", std::string(kLongValueLength, 'x')),
    };
}

[[nodiscard]] QString cell(const QAbstractItemModel* model, int row, int column,
                           const QModelIndex& parent = QModelIndex()) {
    return model->index(row, column, parent).data().toString();
}

} // namespace

class TstMetadataPanel : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void showsFourColumns();
    void showsTopLevelElements();
    void showsUtf8Values();
    void showsSequenceAsTree();
    void showsBinaryPlaceholder();
    void truncatesLongValues();
    void filterByName();
    void filterByTag();
    void filterByValue_caseInsensitive();
    void filterMatchesNestedElement();
    void clearingFilter_restoresAll();
    void setMetadata_replacesPrevious();
    void itemsAreReadOnly();

private:
    void setFilter(const QString& text);

    std::unique_ptr<MetadataPanel> m_panel;
};

void TstMetadataPanel::initTestCase() {
    QLocale::setDefault(QLocale(QLocale::Portuguese, QLocale::Brazil));
}

void TstMetadataPanel::init() {
    m_panel = std::make_unique<MetadataPanel>();
    m_panel->setMetadata(syntheticTree());
}

void TstMetadataPanel::setFilter(const QString& text) {
    auto* filter = m_panel->findChild<QLineEdit*>();
    QVERIFY(filter != nullptr);
    filter->setText(text);
}

void TstMetadataPanel::showsFourColumns() {
    const QAbstractItemModel* model = m_panel->model();
    QCOMPARE(model->columnCount(), 4);
    QCOMPARE(model->headerData(kTag, Qt::Horizontal).toString(), QStringLiteral("Tag"));
    QCOMPARE(model->headerData(kName, Qt::Horizontal).toString(), QStringLiteral("Nome"));
    QCOMPARE(model->headerData(kVr, Qt::Horizontal).toString(), QStringLiteral("VR"));
    QCOMPARE(model->headerData(kValue, Qt::Horizontal).toString(), QStringLiteral("Valor"));
}

void TstMetadataPanel::showsTopLevelElements() {
    const QAbstractItemModel* model = m_panel->model();
    QCOMPARE(model->rowCount(), kRootRows);
    QCOMPARE(cell(model, 0, kTag), QStringLiteral("(0008,0060)"));
    QCOMPARE(cell(model, 0, kName), QStringLiteral("Modality"));
    QCOMPARE(cell(model, 0, kVr), QStringLiteral("CS"));
    QCOMPARE(cell(model, 0, kValue), QStringLiteral("MG"));
    QCOMPARE(cell(model, kPixelDataRow, kTag), QStringLiteral("(7fe0,0010)"));
}

void TstMetadataPanel::showsUtf8Values() {
    QCOMPARE(cell(m_panel->model(), 1, kValue), QStringLiteral("Conceição^João"));
}

void TstMetadataPanel::showsSequenceAsTree() {
    const QAbstractItemModel* model = m_panel->model();
    QCOMPARE(cell(model, kSequenceRow, kVr), QStringLiteral("SQ"));
    QCOMPARE(cell(model, kSequenceRow, kValue), QStringLiteral("<sequência: 2 itens>"));
    const QModelIndex sequence = model->index(kSequenceRow, 0);
    QCOMPARE(model->rowCount(sequence), 2);
    for (int i = 0; i < 2; ++i) {
        QCOMPARE(cell(model, i, kTag, sequence), QStringLiteral("Item %1").arg(i + 1));
        QVERIFY(cell(model, i, kName, sequence).isEmpty());
        const QModelIndex item = model->index(i, 0, sequence);
        QCOMPARE(model->rowCount(item), 1);
        QCOMPARE(cell(model, 0, kTag, item), QStringLiteral("(0008,1150)"));
        QCOMPARE(cell(model, 0, kValue, item), QStringLiteral("1.2.3"));
    }
}

void TstMetadataPanel::showsBinaryPlaceholder() {
    QCOMPARE(cell(m_panel->model(), kPixelDataRow, kValue), QStringLiteral("<binário: 1.024 bytes>"));
}

void TstMetadataPanel::truncatesLongValues() {
    const QModelIndex value = m_panel->model()->index(kLongValueRow, kValue);
    const QString shown = value.data().toString();
    QCOMPARE(shown.size(), kDisplayedLength);
    QVERIFY(shown.endsWith(QChar(0x2026)));
    QCOMPARE(value.data(Qt::UserRole).toString().size(), kLongValueLength);
    QCOMPARE(value.data(Qt::ToolTipRole).toString().size(), kLongValueLength);
}

void TstMetadataPanel::filterByName() {
    setFilter(QStringLiteral("patient"));
    QCOMPARE(m_panel->model()->rowCount(), 1);
    QCOMPARE(cell(m_panel->model(), 0, kName), QStringLiteral("PatientName"));
}

void TstMetadataPanel::filterByTag() {
    setFilter(QStringLiteral("0008,0060"));
    QCOMPARE(m_panel->model()->rowCount(), 1);
    QCOMPARE(cell(m_panel->model(), 0, kName), QStringLiteral("Modality"));
}

void TstMetadataPanel::filterByValue_caseInsensitive() {
    setFilter(QStringLiteral("conceição"));
    QCOMPARE(m_panel->model()->rowCount(), 1);
    QCOMPARE(cell(m_panel->model(), 0, kName), QStringLiteral("PatientName"));
}

void TstMetadataPanel::filterMatchesNestedElement() {
    setFilter(QStringLiteral("1.2.3"));
    const QAbstractItemModel* model = m_panel->model();
    QCOMPARE(model->rowCount(), 1);
    QCOMPARE(cell(model, 0, kName), QStringLiteral("SourceImageSequence"));
    const QModelIndex sequence = model->index(0, 0);
    QCOMPARE(model->rowCount(sequence), 2);
    QCOMPARE(model->rowCount(model->index(0, 0, sequence)), 1);
}

void TstMetadataPanel::clearingFilter_restoresAll() {
    setFilter(QStringLiteral("patient"));
    setFilter(QString());
    QCOMPARE(m_panel->model()->rowCount(), kRootRows);
}

void TstMetadataPanel::setMetadata_replacesPrevious() {
    m_panel->setMetadata(syntheticTree());
    QCOMPARE(m_panel->model()->rowCount(), kRootRows);
    m_panel->clear();
    QCOMPARE(m_panel->model()->rowCount(), 0);
}

void TstMetadataPanel::itemsAreReadOnly() {
    const QAbstractItemModel* model = m_panel->model();
    for (int column = 0; column < model->columnCount(); ++column) {
        QVERIFY(!model->flags(model->index(0, column)).testFlag(Qt::ItemIsEditable));
    }
    const QModelIndex nested = model->index(0, 0, model->index(0, 0, model->index(kSequenceRow, 0)));
    QVERIFY(nested.isValid());
    QVERIFY(!model->flags(nested).testFlag(Qt::ItemIsEditable));
}

QTEST_MAIN(TstMetadataPanel)
#include "tst_MetadataPanel.moc"
