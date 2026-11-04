#include "datasim/intake/builder/candidate_bundle.hpp"
#include "datasim/component/validation/domain_validation.hpp"

namespace datasim::intake {

CandidateBundleResult build_candidate_bundle(const IntakeManifest& manifest) {
    CandidateBundleResult result;
    result.report = validate_intake_manifest(manifest);
    if (!result.report.valid()) {
        return result;
    }

    component::ComponentDomainBundle bundle;
    bundle.component = manifest.component;
    bundle.documents.push_back(manifest.source_document);
    for (const auto& entry : manifest.evidence) {
        bundle.evidence_locations.push_back(entry.location);
    }
    for (const auto& entry : manifest.observations) {
        bundle.observations.push_back(entry.observation);
    }
    const auto domain_report = component::validate_domain_bundle(bundle);
    result.report.domain_validation = domain_report;
    if (!domain_report.valid()) {
        return result;
    }
    result.bundle = std::move(bundle);
    return result;
}

} // namespace datasim::intake
