#pragma once

#include "datasim/intake/intake.hpp"

namespace datasim::intake {

[[nodiscard]] IntakeReport validate_intake_manifest(const IntakeManifest& manifest);

} // namespace datasim::intake
