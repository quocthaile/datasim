#include "intake_fixtures.hpp"
#include "datasim/component/validation/domain_validation.hpp"
#include <cassert>

int main() {
    const auto manifest = datasim::intake::testing::make_synthetic_resistor_manifest();
    const auto candidate = datasim::intake::build_candidate_bundle(manifest);
    assert(candidate.bundle.has_value());
    assert(!candidate.bundle->knowledge.has_value());
    assert(candidate.bundle->models.empty());
    assert(candidate.bundle->model_validations.empty());
    assert(datasim::component::validate_domain_bundle(*candidate.bundle).valid());
    return 0;
}
