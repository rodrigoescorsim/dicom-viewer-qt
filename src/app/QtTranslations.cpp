#include "app/QtTranslations.hpp"

#include <QCoreApplication>
#include <QLibraryInfo>
#include <QLocale>
#include <QTranslator>

#include <memory>

namespace dv::app {

bool installQtTranslations(QCoreApplication& application) {
    // Always Portuguese, whatever the system language. TranslationsPath is the Qt installation during
    // development and the executable's folder after windeployqt.
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
