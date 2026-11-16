#pragma once

#include "core/ImageDocument.hpp"

#include <QWidget>

#include <vector>

class QAbstractItemModel;
class QLineEdit;
class QSortFilterProxyModel;
class QStandardItem;
class QStandardItemModel;
class QTreeView;

namespace dv::app {

/// Every DICOM element of the file as a tree (sequences -> items -> elements), with a text filter.
class MetadataPanel : public QWidget {
    Q_OBJECT

public:
    explicit MetadataPanel(QWidget* parent = nullptr);

    void setMetadata(const std::vector<dv::MetadataNode>& nodes);
    void clear();
    [[nodiscard]] QAbstractItemModel* model() const noexcept; // the filtered (proxy) model; for tests

private:
    void applyFilter(const QString& text);
    void copyCurrentRow() const;

    QLineEdit* m_filter{nullptr};
    QTreeView* m_view{nullptr};
    QStandardItemModel* m_model{nullptr};
    QSortFilterProxyModel* m_proxy{nullptr};
};

} // namespace dv::app
