#include "core/Orientation.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <utility>

namespace dv {

namespace {

constexpr double kMinCosine = 0.25;

constexpr char oppositeLetter(char c) noexcept {
    switch (c) {
    case 'A':
        return 'P';
    case 'P':
        return 'A';
    case 'R':
        return 'L';
    case 'L':
        return 'R';
    case 'H':
        return 'F';
    case 'F':
        return 'H';
    default:
        return '\0';
    }
}

std::string keepDirectionLetters(std::string_view code) {
    std::string out;
    for (const char c : code) {
        if (oppositeLetter(c) != '\0') {
            out.push_back(c);
        }
    }
    return out;
}

} // namespace

std::string oppositeDirection(std::string_view code) {
    std::string out;
    out.reserve(code.size());
    for (const char c : code) {
        if (const char opposite = oppositeLetter(c); opposite != '\0') {
            out.push_back(opposite);
        }
    }
    return out;
}

std::string directionFromCosines(double x, double y, double z) {
    std::array<std::pair<double, char>, 3> axes{{
        {std::abs(x), x < 0.0 ? 'R' : 'L'},
        {std::abs(y), y < 0.0 ? 'A' : 'P'},
        {std::abs(z), z < 0.0 ? 'F' : 'H'},
    }};
    std::ranges::stable_sort(axes, std::ranges::greater{}, &std::pair<double, char>::first);

    std::string out;
    for (const auto& [magnitude, letter] : axes) {
        if (magnitude >= kMinCosine) {
            out.push_back(letter);
        }
    }
    return out;
}

EdgeLabels edgeLabelsFromPatientOrientation(std::string_view rowDirection, std::string_view columnDirection) {
    std::string right = keepDirectionLetters(rowDirection);
    std::string bottom = keepDirectionLetters(columnDirection);
    return {.left = oppositeDirection(right),
            .right = std::move(right),
            .top = oppositeDirection(bottom),
            .bottom = std::move(bottom)};
}

EdgeLabels edgeLabelsFromImageOrientation(std::span<const double, 6> cosines) {
    return edgeLabelsFromPatientOrientation(directionFromCosines(cosines[0], cosines[1], cosines[2]),
                                            directionFromCosines(cosines[3], cosines[4], cosines[5]));
}

} // namespace dv
