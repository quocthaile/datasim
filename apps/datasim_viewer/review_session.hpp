#pragma once

#include "datasim/component/observation/parameter_observation.hpp"
#include "datasim/component/evidence/evidence_location.hpp"

#include <map>
#include <string>
#include <vector>

namespace datasim_viewer {

// Application-owned, volatile review state for one extraction result.
class ReviewSession final {
public:
    ReviewSession(std::vector<datasim::component::ParameterObservation> observations,
                  std::vector<datasim::component::EvidenceLocation> evidence_locations,
                  std::string source_fingerprint,
                  std::string extraction_configuration);

    [[nodiscard]] const datasim::component::ParameterObservation& observation(
        const datasim::component::ObservationId& id) const;
    [[nodiscard]] const std::vector<datasim::component::ObservationId>& observation_ids() const noexcept;
    [[nodiscard]] const std::vector<datasim::component::EvidenceLocation>& evidence_locations() const noexcept;
    [[nodiscard]] std::vector<datasim::component::EvidenceId> evidence_for(
        const datasim::component::ObservationId& id) const;
    [[nodiscard]] const std::string& source_fingerprint() const noexcept;
    [[nodiscard]] const std::string& extraction_configuration() const noexcept;
    [[nodiscard]] bool edited() const noexcept;
    [[nodiscard]] bool valid() const noexcept;

    void apply(const datasim::component::ObservationId& id,
               const datasim::component::ReviewDecision& decision,
               const std::string& decision_context);
    void invalidate() noexcept;

private:
    std::map<datasim::component::ObservationId, datasim::component::ParameterObservation> observations_;
    std::vector<datasim::component::ObservationId> ids_;
    std::vector<datasim::component::EvidenceLocation> evidence_locations_;
    std::string source_fingerprint_;
    std::string extraction_configuration_;
    bool edited_ = false;
    bool valid_ = true;
};

} // namespace datasim_viewer
