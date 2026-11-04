#pragma once

#include "datasim/intake/intake.hpp"

namespace datasim::intake {

[[nodiscard]] CandidateBundleResult build_candidate_bundle(const IntakeManifest& manifest);

} // namespace datasim::intake
