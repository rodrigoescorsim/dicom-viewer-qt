#pragma once

#include "core/ImageDocument.hpp"
#include "core/LoadError.hpp"

#include <QObject>
#include <QString>
#include <QThreadPool>

#include <atomic>
#include <memory>

namespace dv::app {

using DocumentPtr = std::shared_ptr<const dv::ImageDocument>;

/// Loads DICOM files on a worker thread and reports the result on the thread that owns this object.
class DocumentLoader : public QObject {
    Q_OBJECT

public:
    explicit DocumentLoader(QObject* parent = nullptr);
    ~DocumentLoader() override; // blocks until the running load, if any, has finished

    DocumentLoader(const DocumentLoader&) = delete;
    DocumentLoader& operator=(const DocumentLoader&) = delete;
    DocumentLoader(DocumentLoader&&) = delete;
    DocumentLoader& operator=(DocumentLoader&&) = delete;

    /// Starts loading `filePath`. A newer call supersedes the previous ones: only the most recent
    /// request emits loadSucceeded() or loadFailed().
    void load(const QString& filePath);

signals:
    void loadStarted(const QString& filePath);
    void loadSucceeded(const QString& filePath, const dv::app::DocumentPtr& document);
    void loadFailed(const QString& filePath, const dv::LoadError& error);

private:
    QThreadPool m_pool; ///< Single thread: loads run one at a time, in request order.
    std::shared_ptr<std::atomic<quint64>> m_latestRequest; ///< Id of the most recent request.
};

} // namespace dv::app
