#include "app/MetadataPanel.hpp"

#include <QAction>
#include <QClipboard>
#include <QGuiApplication>
#include <QHeaderView>
#include <QKeySequence>
#include <QLineEdit>
#include <QList>
#include <QLocale>
#include <QSortFilterProxyModel>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QStringList>
#include <QTreeView>
#include <QVBoxLayout>

#include <cstdint>
#include <string>

namespace dv::app {

namespace {

constexpr int kTagColumn = 0;
constexpr int kNameColumn = 1;
constexpr int kVrColumn = 2;
constexpr int kValueColumn = 3;
constexpr int kColumnCount = 4;

constexpr qsizetype kMaxDisplayedValue = 200; ///< Longer values are cut in the tree...
constexpr qsizetype kMaxFullValue = 4000;     ///< ...and kept up to this length for the tooltip and "Copiar".
constexpr int kNameColumnWidth = 220;
constexpr int kHexFieldWidth = 4;
constexpr int kHexBase = 16;
constexpr int kFullValueRole = Qt::UserRole;

[[nodiscard]] QString toQString(const std::string& text) {
    return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

[[nodiscard]] QString formatTag(std::uint16_t group, std::uint16_t element) {
    return QStringLiteral("(%1,%2)")
        .arg(group, kHexFieldWidth, kHexBase, QLatin1Char('0'))
        .arg(element, kHexFieldWidth, kHexBase, QLatin1Char('0'));
}

[[nodiscard]] bool isSequence(const dv::MetadataNode& node) {
    return !node.isItem && (node.vr == "SQ" || !node.children.empty());
}

[[nodiscard]] QString describeValue(const dv::MetadataNode& node) {
    if (isSequence(node)) {
        const auto count = static_cast<int>(node.children.size());
        return count == 1 ? MetadataPanel::tr("<sequência: 1 item>")
                          : MetadataPanel::tr("<sequência: %1 itens>").arg(count);
    }
    if (node.isBinary) {
        return node.byteLength == 0
                   ? MetadataPanel::tr("<binário>")
                   : MetadataPanel::tr("<binário: %1 bytes>").arg(QLocale().toString(node.byteLength));
    }
    return toQString(node.value);
}

[[nodiscard]] QStandardItem* readOnlyItem(const QString& text) {
    auto* item = new QStandardItem(text);
    item->setEditable(false);
    return item;
}

/// One row of the tree; `itemNumber` is 1-based and used only for item nodes.
[[nodiscard]] QList<QStandardItem*> createRow(const dv::MetadataNode& node, int itemNumber) {
    if (node.isItem) {
        return {readOnlyItem(MetadataPanel::tr("Item %1").arg(itemNumber)), readOnlyItem({}),
                readOnlyItem({}), readOnlyItem({})};
    }
    const QString value = describeValue(node);
    auto* valueItem =
        readOnlyItem(value.size() > kMaxDisplayedValue ? value.left(kMaxDisplayedValue) + QChar(0x2026) // "…"
                                                       : value);
    const QString fullValue = value.left(kMaxFullValue);
    valueItem->setData(fullValue, kFullValueRole);
    if (value.size() > kMaxDisplayedValue) {
        valueItem->setToolTip(fullValue);
    }
    return {readOnlyItem(formatTag(node.group, node.element)), readOnlyItem(toQString(node.name)),
            readOnlyItem(isSequence(node) ? QStringLiteral("SQ") : toQString(node.vr)), valueItem};
}

void appendNodes(QStandardItem* parent, const std::vector<dv::MetadataNode>& nodes) {
    int itemNumber = 0;
    for (const dv::MetadataNode& node : nodes) {
        if (node.isItem) {
            ++itemNumber;
        }
        const QList<QStandardItem*> row = createRow(node, itemNumber);
        parent->appendRow(row);
        appendNodes(row.front(), node.children);
    }
}

} // namespace

MetadataPanel::MetadataPanel(QWidget* parent)
    : QWidget(parent)
    , m_filter(new QLineEdit(this))
    , m_view(new QTreeView(this))
    , m_model(new QStandardItemModel(0, kColumnCount, this))
    , m_proxy(new QSortFilterProxyModel(this)) {
    m_model->setHorizontalHeaderLabels({tr("Tag"), tr("Nome"), tr("VR"), tr("Valor")});

    m_proxy->setSourceModel(m_model);
    m_proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_proxy->setFilterKeyColumn(-1); // all columns
    m_proxy->setRecursiveFilteringEnabled(true);
    m_proxy->setAutoAcceptChildRows(true);

    m_view->setModel(m_proxy);
    m_view->setUniformRowHeights(true);
    m_view->setAlternatingRowColors(true);
    m_view->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_view->setRootIsDecorated(true);
    QHeaderView* header = m_view->header();
    header->setSectionResizeMode(kTagColumn, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(kNameColumn, QHeaderView::Interactive);
    header->setSectionResizeMode(kVrColumn, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(kValueColumn, QHeaderView::Stretch);
    header->resizeSection(kNameColumn, kNameColumnWidth);

    m_filter->setPlaceholderText(tr("Filtrar por tag, nome ou valor…"));
    m_filter->setClearButtonEnabled(true);
    connect(m_filter, &QLineEdit::textChanged, this, &MetadataPanel::applyFilter);

    auto* copyAction = new QAction(tr("Copiar"), m_view);
    copyAction->setShortcut(QKeySequence::Copy);
    copyAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    connect(copyAction, &QAction::triggered, this, &MetadataPanel::copyCurrentRow);
    m_view->addAction(copyAction);
    m_view->setContextMenuPolicy(Qt::ActionsContextMenu);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_filter);
    layout->addWidget(m_view);
}

void MetadataPanel::setMetadata(const std::vector<dv::MetadataNode>& nodes) {
    m_view->setUpdatesEnabled(false);
    m_model->removeRows(0, m_model->rowCount());
    appendNodes(m_model->invisibleRootItem(), nodes);
    if (m_filter->text().isEmpty()) {
        m_view->collapseAll(); // sequences start collapsed
    } else {
        m_view->expandAll();
    }
    m_view->setUpdatesEnabled(true);
}

void MetadataPanel::clear() {
    m_model->removeRows(0, m_model->rowCount());
}

QAbstractItemModel* MetadataPanel::model() const noexcept {
    return m_proxy;
}

void MetadataPanel::applyFilter(const QString& text) {
    m_proxy->setFilterFixedString(text);
    if (text.isEmpty()) {
        m_view->collapseAll();
    } else {
        m_view->expandAll();
    }
}

void MetadataPanel::copyCurrentRow() const {
    const QModelIndex current = m_view->currentIndex();
    if (!current.isValid()) {
        return;
    }
    QStringList fields;
    for (int column = 0; column < kColumnCount; ++column) {
        const QModelIndex index = current.siblingAtColumn(column);
        const QVariant full = index.data(kFullValueRole);
        fields << (full.isValid() ? full.toString() : index.data(Qt::DisplayRole).toString());
    }
    QGuiApplication::clipboard()->setText(fields.join(QLatin1Char('\t')));
}

} // namespace dv::app
