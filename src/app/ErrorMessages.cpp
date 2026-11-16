#include "app/ErrorMessages.hpp"

#include <QCoreApplication>

namespace dv::app {

QString describeLoadError(const dv::LoadError& error) {
    // No `default`: the compiler warns when a new LoadErrorCode is left without a message.
    switch (error.code) {
    case dv::LoadErrorCode::FileNotFound:
        return QCoreApplication::translate("LoadError", "Arquivo não encontrado.");
    case dv::LoadErrorCode::FileNotReadable:
        return QCoreApplication::translate("LoadError",
                                           "Não foi possível ler o arquivo. Verifique se ele não é "
                                           "uma pasta e se você tem permissão de leitura.");
    case dv::LoadErrorCode::NotDicom:
        return QCoreApplication::translate("LoadError",
                                           "O arquivo selecionado não é um arquivo DICOM válido.");
    case dv::LoadErrorCode::NoPixelData:
        return QCoreApplication::translate("LoadError", "O arquivo é DICOM, mas não contém uma imagem (por "
                                                        "exemplo, um laudo estruturado ou um DICOMDIR).");
    case dv::LoadErrorCode::UnsupportedTransferSyntax:
        if (error.detail.empty()) {
            return QCoreApplication::translate("LoadError", "A imagem usa uma compressão não suportada.");
        }
        return QCoreApplication::translate("LoadError", "A imagem usa uma compressão não suportada: %1.")
            .arg(QString::fromUtf8(error.detail.data(), static_cast<qsizetype>(error.detail.size())));
    case dv::LoadErrorCode::UnsupportedImage:
        return QCoreApplication::translate("LoadError", "O formato desta imagem não é suportado.");
    case dv::LoadErrorCode::CorruptPixelData:
        return QCoreApplication::translate("LoadError",
                                           "Os dados da imagem estão corrompidos ou incompletos.");
    case dv::LoadErrorCode::OutOfMemory:
        return QCoreApplication::translate("LoadError", "Memória insuficiente para carregar a imagem.");
    }
    // Only reachable with a value outside the enumeration.
    return QCoreApplication::translate("LoadError", "O formato desta imagem não é suportado.");
}

} // namespace dv::app
