#include "app/WindowLevelControls.hpp"

#include "core/ImageDocument.hpp"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QSignalBlocker>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <initializer_list>
#include <string>

namespace dv::app {

namespace {

constexpr double kCenterLimit = 1e7;
constexpr double kMaxWidth = 1e7;
constexpr double kStepDivisor = 100.0;   ///< One spin box step = 1 % of the image's value range.
constexpr double kPresetTolerance = 0.5; ///< A window matches a preset when both values are this close.
constexpr int kSpinBoxMinimumWidth = 90;

[[nodiscard]] QString toQString(const std::string& text) {
    return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

[[nodiscard]] QDoubleSpinBox* createSpinBox(QWidget* parent, double minimum, double maximum) {
    auto* spinBox = new QDoubleSpinBox(parent);
    spinBox->setRange(minimum, maximum);
    spinBox->setKeyboardTracking(false);
    spinBox->setAccelerated(true);
    spinBox->setMinimumWidth(kSpinBoxMinimumWidth);
    return spinBox;
}

} // namespace

WindowLevelControls::WindowLevelControls(QWidget* parent)
    : QWidget(parent)
    , m_presets(new QComboBox(this))
    , m_center(createSpinBox(this, -kCenterLimit, kCenterLimit))
    , m_width(createSpinBox(this, dv::kMinWindowWidth, kMaxWidth)) {
    m_center->setObjectName(QStringLiteral("windowCenter"));
    m_width->setObjectName(QStringLiteral("windowWidth"));
    m_presets->setObjectName(QStringLiteral("windowPresets"));
    m_presets->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    m_presets->setPlaceholderText(tr("Personalizada"));

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(new QLabel(tr("Janela:"), this));
    layout->addWidget(m_presets);
    layout->addWidget(new QLabel(tr("WL:"), this));
    layout->addWidget(m_center);
    layout->addWidget(new QLabel(tr("WW:"), this));
    layout->addWidget(m_width);

    // activated, not currentIndexChanged: only a choice made by the user is an edit.
    connect(m_presets, &QComboBox::activated, this, &WindowLevelControls::onPresetActivated);
    connect(m_center, &QDoubleSpinBox::valueChanged, this, &WindowLevelControls::onSpinBoxEdited);
    connect(m_width, &QDoubleSpinBox::valueChanged, this, &WindowLevelControls::onSpinBoxEdited);

    setDocument(nullptr);
}

void WindowLevelControls::setDocument(const DocumentPtr& document) {
    const QSignalBlocker blockPresets(m_presets);
    const QSignalBlocker blockCenter(m_center);
    const QSignalBlocker blockWidth(m_width);
    m_presets->clear();
    m_presetWindows.clear();

    const dv::MonoImage* mono = document ? document->mono() : nullptr;
    const bool enabled = mono != nullptr;
    for (QWidget* control : std::initializer_list<QWidget*>{m_presets, m_center, m_width}) {
        control->setEnabled(enabled);
        control->setToolTip(enabled ? QString()
                                    : tr("O ajuste de janela se aplica apenas a imagens em tons de cinza."));
    }
    if (!enabled) {
        m_center->clear(); // no stale numbers next to a color image
        m_width->clear();
        return;
    }

    const int decimals = mono->mapping.scale == 1.0 ? 0 : 1;
    const double step = std::max(1.0, (mono->maxValue - mono->minValue) / kStepDivisor);
    for (QDoubleSpinBox* spinBox : {m_center, m_width}) {
        spinBox->setDecimals(decimals);
        spinBox->setSingleStep(step);
    }

    for (std::size_t i = 0; i < mono->presets.size(); ++i) {
        const dv::WindowPreset& preset = mono->presets[i];
        const QString label =
            preset.label.empty() ? tr("Preset %1").arg(static_cast<int>(i) + 1) : toQString(preset.label);
        m_presets->addItem(tr("%1 (WL %2 / WW %3)")
                               .arg(label, QString::number(preset.window.center, 'f', decimals),
                                    QString::number(preset.window.width, 'f', decimals)));
        m_presetWindows.push_back(preset.window);
    }
    m_presets->addItem(tr("Faixa completa"));
    m_presetWindows.push_back(dv::fullRangeWindow(mono->minValue, mono->maxValue));
    setWindow(mono->defaultWindow); // the window the viewport starts with
}

void WindowLevelControls::setWindow(const dv::VoiWindow& window) {
    if (m_presetWindows.empty()) {
        return; // no monochrome document (the list always holds "Faixa completa" when there is one)
    }
    const QSignalBlocker blockPresets(m_presets);
    const QSignalBlocker blockCenter(m_center);
    const QSignalBlocker blockWidth(m_width);
    m_current = window;
    m_center->setValue(window.center);
    m_width->setValue(window.width);

    const auto matches = [&window](const dv::VoiWindow& preset) {
        return std::abs(preset.center - window.center) < kPresetTolerance &&
               std::abs(preset.width - window.width) < kPresetTolerance;
    };
    const auto found = std::ranges::find_if(m_presetWindows, matches);
    m_presets->setCurrentIndex(found == m_presetWindows.end()
                                   ? -1 // shows the "Personalizada" placeholder
                                   : static_cast<int>(found - m_presetWindows.begin()));
}

void WindowLevelControls::onPresetActivated(int index) {
    if (index >= 0 && static_cast<std::size_t>(index) < m_presetWindows.size()) {
        emit windowEdited(m_presetWindows[static_cast<std::size_t>(index)]);
    }
}

void WindowLevelControls::onSpinBoxEdited() {
    emit windowEdited(
        {.center = m_center->value(), .width = m_width->value(), .function = m_current.function});
}

} // namespace dv::app
