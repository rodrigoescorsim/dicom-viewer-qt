#include "app/DocumentLoader.hpp"

#include "dicom/DicomLoader.hpp"

#include <QFileInfo>
#include <QFuture>
#include <QtConcurrent/QtConcurrentRun>

#include <filesystem>
#include <utility>

namespace dv::app {

namespace {
using LoadOutcome = dv::LoadResult<DocumentPtr>;
} // namespace

DocumentLoader::DocumentLoader(QObject* parent)
    : QObject(parent)
    , m_latestRequest(std::make_shared<std::atomic<quint64>>(0)) {
    m_pool.setMaxThreadCount(1);
}

DocumentLoader::~DocumentLoader() {
    m_latestRequest->fetch_add(1); // queued requests now return immediately
    m_pool.waitForDone();          // the running one must end before the codecs are unregistered
}

void DocumentLoader::load(const QString& filePath) {
    const quint64 requestId = m_latestRequest->fetch_add(1) + 1;
    emit loadStarted(filePath);

    const std::filesystem::path path = QFileInfo(filePath).filesystemAbsoluteFilePath();
    QtConcurrent::run(&m_pool, [path, requestId, latest = m_latestRequest]() -> LoadOutcome {
        if (latest->load() != requestId) {
            return std::unexpected(dv::LoadError{}); // superseded while queued; never delivered
        }
        return dv::loadDicomFile(path).transform([](dv::ImageDocument&& document) -> DocumentPtr {
            return std::make_shared<const dv::ImageDocument>(std::move(document));
        });
    }).then(this, [this, filePath, requestId](const LoadOutcome& outcome) {
        if (m_latestRequest->load() != requestId) {
            return; // a newer request exists: drop this result
        }
        if (outcome) {
            emit loadSucceeded(filePath, *outcome);
        } else {
            emit loadFailed(filePath, outcome.error());
        }
    });
}

} // namespace dv::app
