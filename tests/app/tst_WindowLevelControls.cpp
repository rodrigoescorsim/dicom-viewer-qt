#include "app/ImageViewport.hpp"
#include "app/WindowLevelControls.hpp"
#include "core/ImageDocument.hpp"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QTest>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>
#include <variant>
#include <vector>

#include "support/TestDocuments.hpp"

using dv::VoiFunction;
using dv::VoiWindow;
using dv::app::DocumentPtr;
using dv::app::ImageViewport;
using dv::app::WindowLevelControls;

namespace {

constexpr double kMinValue = 1061.0;
constexpr double kMaxValue = 3541.0;
constexpr VoiWindow kSofter{.center = 3091.0, .width = 1150.0, .function = VoiFunction::Sigmoid};
constexpr VoiWindow kNormal{.center = 3145.0, .width = 1000.0, .function = VoiFunction::Sigmoid};
constexpr VoiWindow kUnnamed{.center = 3181.0, .width = 850.0, .function = VoiFunction::Sigmoid};
constexpr VoiWindow kFullRange{.center = 2301.5, .width = 2481.0, .function = VoiFunction::Linear};

/// Mono document shaped like the mammography test file: three presets (the last one unnamed), SIGMOID.
[[nodiscard]] DocumentPtr mammographyLikeDocument() {
    std::vector<std::uint16_t> indices(16, static_cast<std::uint16_t>(kMinValue));
    indices.back() = static_cast<std::uint16_t>(kMaxValue);
    dv::ImageDocument document = *dv::test::makeMonoDocument(4, 4, std::move(indices), kSofter);
    auto& mono = std::get<dv::MonoImage>(document.pixels);
    mono.presets = {{.label = "SOFTER", .window = kSofter},
                    {.label = "NORMAL", .window = kNormal},
                    {.label = "", .window = kUnnamed}};
    return std::make_shared<const dv::ImageDocument>(std::move(document));
}

[[nodiscard]] DocumentPtr colorDocument() {
    return dv::test::makeColorDocument(2, 1, {255, 0, 0, 0, 0, 255});
}

/// Records every windowEdited() emission.
struct EditRecorder {
    explicit EditRecorder(WindowLevelControls& controls) {
        QObject::connect(&controls, &WindowLevelControls::windowEdited, &controls,
                         [this](const VoiWindow& window) { edits.push_back(window); });
    }
    std::vector<VoiWindow> edits;
};

} // namespace

class TstWindowLevelControls : public QObject {
    Q_OBJECT

private slots:
    void listsPresetsAndFullRange();
    void setWindow_updatesWithoutEmitting();
    void customWindow_showsPlaceholder();
    void pickingPreset_emits();
    void pickingFullRange_emitsLinear();
    void editingSpin_emitsWithCurrentFunction();
    void colorDocument_disables();
    void noSignalLoop();

private:
    [[nodiscard]] static QComboBox* presets(WindowLevelControls& controls);
    [[nodiscard]] static QDoubleSpinBox* centerSpin(WindowLevelControls& controls);
    [[nodiscard]] static QDoubleSpinBox* widthSpin(WindowLevelControls& controls);
};

QComboBox* TstWindowLevelControls::presets(WindowLevelControls& controls) {
    return controls.findChild<QComboBox*>(QStringLiteral("windowPresets"));
}

QDoubleSpinBox* TstWindowLevelControls::centerSpin(WindowLevelControls& controls) {
    return controls.findChild<QDoubleSpinBox*>(QStringLiteral("windowCenter"));
}

QDoubleSpinBox* TstWindowLevelControls::widthSpin(WindowLevelControls& controls) {
    return controls.findChild<QDoubleSpinBox*>(QStringLiteral("windowWidth"));
}

void TstWindowLevelControls::listsPresetsAndFullRange() {
    WindowLevelControls controls;
    controls.setDocument(mammographyLikeDocument());

    QComboBox* combo = presets(controls);
    QVERIFY(combo != nullptr);
    QCOMPARE(combo->count(), 4);
    QCOMPARE(combo->itemText(0), QStringLiteral("SOFTER (WL 3091 / WW 1150)"));
    QCOMPARE(combo->itemText(1), QStringLiteral("NORMAL (WL 3145 / WW 1000)"));
    QCOMPARE(combo->itemText(2), QStringLiteral("Preset 3 (WL 3181 / WW 850)"));
    QCOMPARE(combo->itemText(3), QStringLiteral("Faixa completa"));
}

void TstWindowLevelControls::setWindow_updatesWithoutEmitting() {
    WindowLevelControls controls;
    controls.setDocument(mammographyLikeDocument());
    EditRecorder recorder(controls);

    controls.setWindow(kNormal);
    QCOMPARE(centerSpin(controls)->value(), 3145.0);
    QCOMPARE(widthSpin(controls)->value(), 1000.0);
    QCOMPARE(presets(controls)->currentIndex(), 1);
    QVERIFY(recorder.edits.empty());
}

void TstWindowLevelControls::customWindow_showsPlaceholder() {
    WindowLevelControls controls;
    controls.setDocument(mammographyLikeDocument());

    controls.setWindow({.center = 2000.0, .width = 300.0, .function = VoiFunction::Sigmoid});
    QCOMPARE(presets(controls)->currentIndex(), -1);
    QCOMPARE(presets(controls)->placeholderText(), QStringLiteral("Personalizada"));
}

void TstWindowLevelControls::pickingPreset_emits() {
    WindowLevelControls controls;
    controls.setDocument(mammographyLikeDocument());
    EditRecorder recorder(controls);

    emit presets(controls)->activated(2);
    QCOMPARE(recorder.edits.size(), std::size_t{1});
    QCOMPARE(recorder.edits.front(), kUnnamed);
}

void TstWindowLevelControls::pickingFullRange_emitsLinear() {
    WindowLevelControls controls;
    controls.setDocument(mammographyLikeDocument());
    EditRecorder recorder(controls);

    emit presets(controls)->activated(3);
    QCOMPARE(recorder.edits.size(), std::size_t{1});
    QCOMPARE(recorder.edits.front(), kFullRange);
}

void TstWindowLevelControls::editingSpin_emitsWithCurrentFunction() {
    WindowLevelControls controls;
    controls.setDocument(mammographyLikeDocument());
    controls.setWindow(kSofter);
    EditRecorder recorder(controls);

    widthSpin(controls)->setValue(700.0);
    QCOMPARE(recorder.edits.size(), std::size_t{1});
    QCOMPARE(recorder.edits.front().width, 700.0);
    QCOMPARE(recorder.edits.front().center, kSofter.center);
    QVERIFY(recorder.edits.front().function == VoiFunction::Sigmoid);
}

void TstWindowLevelControls::colorDocument_disables() {
    WindowLevelControls controls;
    controls.setDocument(mammographyLikeDocument());
    QVERIFY(presets(controls)->isEnabled());

    controls.setDocument(colorDocument());
    QVERIFY(!presets(controls)->isEnabled());
    QVERIFY(!centerSpin(controls)->isEnabled());
    QVERIFY(!widthSpin(controls)->isEnabled());
    QVERIFY(!presets(controls)->toolTip().isEmpty());
}

void TstWindowLevelControls::noSignalLoop() {
    ImageViewport viewport;
    WindowLevelControls controls;
    connect(&viewport, &ImageViewport::windowChanged, &controls, &WindowLevelControls::setWindow);
    connect(&controls, &WindowLevelControls::windowEdited, &viewport, &ImageViewport::setWindow);
    viewport.resize(400, 300);
    viewport.show();
    QVERIFY(QTest::qWaitForWindowExposed(&viewport));

    const DocumentPtr document = mammographyLikeDocument();
    controls.setDocument(document);
    viewport.setDocument(document);
    int windowSignals = 0;
    connect(&viewport, &ImageViewport::windowChanged, this, [&windowSignals] { ++windowSignals; });
    EditRecorder recorder(controls);

    constexpr int kMoves = 3;
    QTest::mousePress(&viewport, Qt::LeftButton, Qt::NoModifier, QPoint(100, 100));
    for (int i = 1; i <= kMoves; ++i) {
        QTest::mouseMove(&viewport, QPoint(100 + 10 * i, 100));
    }
    QTest::mouseRelease(&viewport, Qt::LeftButton, Qt::NoModifier, QPoint(100 + 10 * kMoves, 100));

    QCOMPARE(windowSignals, kMoves);
    QVERIFY(recorder.edits.empty());
    QCOMPARE(widthSpin(controls)->value(), std::round(viewport.window().width)); // 0 decimals shown
}

QTEST_MAIN(TstWindowLevelControls)
#include "tst_WindowLevelControls.moc"
