#pragma once

class QCoreApplication;

namespace dv::app {

/// Installs Qt's Brazilian Portuguese catalog (standard dialog buttons, context menus...).
/// Returns false when the catalog is not found.
[[nodiscard]] bool installQtTranslations(QCoreApplication& application);

} // namespace dv::app
