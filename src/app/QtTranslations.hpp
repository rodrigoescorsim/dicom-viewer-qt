#pragma once

class QCoreApplication;

namespace dv::app {

/// Installs Qt's own Brazilian Portuguese translations (buttons of standard dialogs, context menus...),
/// so the whole interface is in Portuguese. Returns false when the catalog is not found; the application
/// still works, with those few built-in texts in English.
[[nodiscard]] bool installQtTranslations(QCoreApplication& application);

} // namespace dv::app
