#pragma once

#include <cstdint>
#include <string>

namespace dv {

/// VOI LUT Function (0028,1056). DICOM PS3.3 C.11.2.1.2 / C.11.2.1.3.
enum class VoiFunction : std::uint8_t { Linear, LinearExact, Sigmoid };

/// Window Center (0028,1050) / Window Width (0028,1051), in modality (post Modality LUT) units.
struct VoiWindow {
    double center{0.0};
    double width{1.0};
    VoiFunction function{VoiFunction::Linear};

    friend bool operator==(const VoiWindow&, const VoiWindow&) = default;
};

/// A window shipped with the file plus its Window Center & Width Explanation (0028,1055), if any.
struct WindowPreset {
    std::string label; ///< UTF-8; empty when the file gives no explanation.
    VoiWindow window;
};

/// Smallest width accepted by the application, for every VOI function.
inline constexpr double kMinWindowWidth = 1.0;

/// Returns `window` with a finite center and a finite width >= kMinWindowWidth.
[[nodiscard]] VoiWindow sanitized(VoiWindow window) noexcept;

/// Maps modality value `x` to a normalized display value in [0, 1].
/// Precondition: `window` is sanitized.
[[nodiscard]] double evaluateVoi(const VoiWindow& window, double x) noexcept;

/// Linear window that maps exactly [minValue, maxValue] to [0, 1].
[[nodiscard]] VoiWindow fullRangeWindow(double minValue, double maxValue) noexcept;

} // namespace dv
