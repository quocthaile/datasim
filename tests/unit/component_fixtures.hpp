#pragma once
#include "datasim/component/validation/domain_validation.hpp"

namespace datasim::component::testing {

// Synthetic passive component bundles for domain contract verification only.
// Marked is_synthetic = true; contain no real manufacturer datasheet data or RC run outputs.
ComponentDomainBundle make_synthetic_resistor_bundle();
ComponentDomainBundle make_synthetic_capacitor_bundle();

} // namespace datasim::component::testing
