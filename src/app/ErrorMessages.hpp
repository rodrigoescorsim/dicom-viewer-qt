#pragma once

#include "core/LoadError.hpp"

#include <QString>

namespace dv::app {

/// Portuguese, user-facing sentence for a load failure. Never empty.
[[nodiscard]] QString describeLoadError(const dv::LoadError& error);

} // namespace dv::app
