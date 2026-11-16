#pragma once

#include "app/DocumentLoader.hpp" // DocumentPtr
#include "core/Voi.hpp"

#include <QWidget>

#include <vector>

class QComboBox;
class QDoubleSpinBox;

namespace dv::app {

/// Window presets of the file plus numeric Window Level / Window Width fields (placed in the tool bar).
class WindowLevelControls : public QWidget {
    Q_OBJECT

public:
    explicit WindowLevelControls(QWidget* parent = nullptr);

    /// Fills the preset list from `document` and enables the controls for monochrome images.
    /// nullptr or a color document disables everything.
    void setDocument(const DocumentPtr& document);

public slots:
    /// Reflects `window` in the controls WITHOUT emitting windowEdited().
    void setWindow(const dv::VoiWindow& window);

signals:
    /// The user changed the window through a spin box or by picking a preset.
    void windowEdited(const dv::VoiWindow& window);

private:
    void onPresetActivated(int index);
    void onSpinBoxEdited();

    QComboBox* m_presets{nullptr};
    QDoubleSpinBox* m_center{nullptr};
    QDoubleSpinBox* m_width{nullptr};
    std::vector<dv::VoiWindow> m_presetWindows; ///< Parallel to the combo box items.
    dv::VoiWindow m_current;
};

} // namespace dv::app
