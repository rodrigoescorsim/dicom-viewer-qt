#include "app/QtTranslations.hpp"

#include <QCoreApplication>
#include <QLibraryInfo>
#include <QLocale>
#include <QTranslator>

#include <memory>

namespace dv::app {

bool installQtTranslations(QCoreApplication& application) {
    // The interface is always Portuguese, whatever the system language. QLibraryInfo points to the Qt
    // installation during development and to the folder next to the executable after windeployqt.
    auto translator = std::make_unique<QTranslator>();
    if (!translator->load(QLocale(QLocale::Portuguese, QLocale::Brazil), QStringLiteral("qt"),
                          QStringLiteral("_"), QLibraryInfo::path(QLibraryInfo::TranslationsPath))) {
        return false;
    }
    QTranslator* owned = translator.release();
    owned->setParent(&application); // owned by the application from here on
    return QCoreApplication::installTranslator(owned);
}

} // namespace dv::app
