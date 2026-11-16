#include "app/DocumentLoader.hpp"
#include "app/ErrorMessages.hpp"
#include "dicom/CodecRegistry.hpp"

#include <QSet>
#include <QString>
#include <QTest>
#include <QThread>

#include <array>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include "support/TestSupport.hpp"

using dv::LoadErrorCode;
using dv::app::DocumentLoader;
using dv::app::DocumentPtr;

namespace {

constexpr int kLoadTimeoutMs = 10000;
constexpr int kSettleTimeMs = 200;
constexpr int kSmallSide = 64;

[[nodiscard]] QString sampleFile(std::string_view fileName) {
    return QString::fromStdU16String(dv::test::samplePath(fileName).u16string());
}

/// Records every signal of a DocumentLoader (QSignalSpy is avoided for the project's own types).
struct LoaderRecorder {
    explicit LoaderRecorder(DocumentLoader& loader) {
        QObject::connect(&loader, &DocumentLoader::loadStarted, &loader,
                         [this](const QString& filePath) { started.push_back(filePath); });
        QObject::connect(&loader, &DocumentLoader::loadSucceeded, &loader,
                         [this](const QString& filePath, const DocumentPtr& document) {
                             succeeded.push_back(filePath);
                             lastDocument = document;
                             signalThread = QThread::currentThread();
                         });
        QObject::connect(&loader, &DocumentLoader::loadFailed, &loader,
                         [this](const QString& filePath, const dv::LoadError& error) {
                             failed.push_back(filePath);
                             lastError = error;
                             signalThread = QThread::currentThread();
                         });
    }

    [[nodiscard]] qsizetype resultCount() const noexcept { return succeeded.size() + failed.size(); }

    QStringList started;
    QStringList succeeded;
    QStringList failed;
    DocumentPtr lastDocument;
    std::optional<dv::LoadError> lastError;
    QThread* signalThread{nullptr};
};

} // namespace

class TstDocumentLoader : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void loadsValidFile();
    void reportsFailure();
    void reportsMissingFile();
    void onlyLatestRequestIsDelivered();
    void signalsArriveOnOwnerThread();
    void destructorWaitsForRunningLoad();
    void describeLoadError_coversEveryCode();

private:
    std::unique_ptr<dv::CodecRegistry> m_codecs;
};

void TstDocumentLoader::initTestCase() {
    m_codecs = std::make_unique<dv::CodecRegistry>();
}

void TstDocumentLoader::loadsValidFile() {
    DocumentLoader loader;
    LoaderRecorder recorder(loader);
    loader.load(sampleFile("MR_small.dcm"));

    QTRY_VERIFY_WITH_TIMEOUT(recorder.resultCount() > 0, kLoadTimeoutMs);
    QCOMPARE(recorder.started.size(), 1);
    QCOMPARE(recorder.succeeded.size(), 1);
    QCOMPARE(recorder.failed.size(), 0);
    QVERIFY(recorder.lastDocument != nullptr);
    QCOMPARE(recorder.lastDocument->width, kSmallSide);
    QCOMPARE(recorder.lastDocument->height, kSmallSide);
}

void TstDocumentLoader::reportsFailure() {
    DocumentLoader loader;
    LoaderRecorder recorder(loader);
    loader.load(sampleFile("not_dicom.dcm"));

    QTRY_VERIFY_WITH_TIMEOUT(recorder.resultCount() > 0, kLoadTimeoutMs);
    QCOMPARE(recorder.failed.size(), 1);
    QCOMPARE(recorder.succeeded.size(), 0);
    QVERIFY(recorder.lastError.has_value());
    QVERIFY(recorder.lastError->code == LoadErrorCode::NotDicom);
}

void TstDocumentLoader::reportsMissingFile() {
    DocumentLoader loader;
    LoaderRecorder recorder(loader);
    loader.load(sampleFile("nao_existe.dcm"));

    QTRY_VERIFY_WITH_TIMEOUT(recorder.resultCount() > 0, kLoadTimeoutMs);
    QCOMPARE(recorder.failed.size(), 1);
    QVERIFY(recorder.lastError.has_value());
    QVERIFY(recorder.lastError->code == LoadErrorCode::FileNotFound);
}

void TstDocumentLoader::onlyLatestRequestIsDelivered() {
    DocumentLoader loader;
    LoaderRecorder recorder(loader);
    loader.load(sampleFile("anonymized_mamo.dcm"));
    loader.load(sampleFile("MR_small.dcm"));

    QTRY_VERIFY_WITH_TIMEOUT(recorder.resultCount() > 0, kLoadTimeoutMs);
    QTest::qWait(kSettleTimeMs); // give a stale result the chance to (wrongly) arrive
    QCOMPARE(recorder.started.size(), 2);
    QCOMPARE(recorder.resultCount(), 1);
    QCOMPARE(recorder.succeeded.size(), 1);
    QCOMPARE(recorder.succeeded.constFirst(), sampleFile("MR_small.dcm"));
    QVERIFY(recorder.lastDocument != nullptr);
    QCOMPARE(recorder.lastDocument->width, kSmallSide);
    QCOMPARE(recorder.lastDocument->height, kSmallSide);
}

void TstDocumentLoader::signalsArriveOnOwnerThread() {
    DocumentLoader loader;
    LoaderRecorder recorder(loader);
    loader.load(sampleFile("MR_small.dcm"));

    QTRY_VERIFY_WITH_TIMEOUT(recorder.resultCount() > 0, kLoadTimeoutMs);
    QCOMPARE(recorder.signalThread, loader.thread());
}

void TstDocumentLoader::destructorWaitsForRunningLoad() {
    std::vector<QString> delivered;
    {
        DocumentLoader loader;
        QObject::connect(
            &loader, &DocumentLoader::loadSucceeded, &loader,
            [&delivered](const QString& filePath, const DocumentPtr&) { delivered.push_back(filePath); });
        QObject::connect(
            &loader, &DocumentLoader::loadFailed, &loader,
            [&delivered](const QString& filePath, const dv::LoadError&) { delivered.push_back(filePath); });
        loader.load(sampleFile("anonymized_mamo.dcm"));
    } // destroyed without processing events
    QTest::qWait(kSettleTimeMs);
    QVERIFY(delivered.empty());
}

void TstDocumentLoader::describeLoadError_coversEveryCode() {
    constexpr std::array kCodes{
        LoadErrorCode::FileNotFound,
        LoadErrorCode::FileNotReadable,
        LoadErrorCode::NotDicom,
        LoadErrorCode::NoPixelData,
        LoadErrorCode::UnsupportedTransferSyntax,
        LoadErrorCode::UnsupportedImage,
        LoadErrorCode::CorruptPixelData,
        LoadErrorCode::OutOfMemory,
    };
    QSet<QString> texts;
    for (const LoadErrorCode code : kCodes) {
        const QString text = dv::app::describeLoadError(dv::LoadError{.code = code, .detail = {}});
        QVERIFY(!text.isEmpty());
        texts.insert(text);
    }
    QCOMPARE(texts.size(), static_cast<qsizetype>(kCodes.size()));

    const QString withDetail = dv::app::describeLoadError(
        dv::LoadError{.code = LoadErrorCode::UnsupportedTransferSyntax, .detail = "JPEG XL"});
    QVERIFY(withDetail.contains(QStringLiteral("JPEG XL")));
}

QTEST_GUILESS_MAIN(TstDocumentLoader)
#include "tst_DocumentLoader.moc"
