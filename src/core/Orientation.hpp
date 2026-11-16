#pragma once

#include <span>
#include <string>
#include <string_view>

namespace dv {

/// Patient direction shown at each edge of the displayed image: "A", "P", "R", "L", "H", "F" or a
/// combination such as "FR". All four strings are empty when the file carries no orientation.
struct EdgeLabels {
    std::string left;
    std::string right;
    std::string top;
    std::string bottom;

    [[nodiscard]] bool empty() const noexcept {
        return left.empty() && right.empty() && top.empty() && bottom.empty();
    }
    friend bool operator==(const EdgeLabels&, const EdgeLabels&) = default;
};

/// "A" -> "P", "FR" -> "HL". Characters other than A, P, R, L, H, F are dropped.
[[nodiscard]] std::string oppositeDirection(std::string_view code);

/// Direction code of a direction cosine in the DICOM patient coordinate system (+x = L, +y = P, +z = H).
/// Components with magnitude < 0.25 are ignored; the remaining letters are ordered by decreasing magnitude.
[[nodiscard]] std::string directionFromCosines(double x, double y, double z);

/// Patient Orientation (0020,0020). `rowDirection` is the direction of increasing column index (towards the
/// right edge); `columnDirection` is the direction of increasing row index (towards the bottom edge).
[[nodiscard]] EdgeLabels edgeLabelsFromPatientOrientation(std::string_view rowDirection,
                                                          std::string_view columnDirection);

/// Image Orientation (Patient) (0020,0037): row cosines (x, y, z) followed by column cosines (x, y, z).
[[nodiscard]] EdgeLabels edgeLabelsFromImageOrientation(std::span<const double, 6> cosines);

} // namespace dv
