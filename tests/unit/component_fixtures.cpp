#include "component_fixtures.hpp"
#include "datasim/component/knowledge/component_knowledge.hpp"
#include "datasim/component/knowledge/normalized_parameter.hpp"
#include "datasim/component/observation/review.hpp"

namespace datasim::component::testing {

namespace {

void promote_observation_through_review(
    ParameterObservation& obs,
    const std::string& intake_decision_id,
    const std::string& approval_decision_id,
    const ReviewerIdentity& engineer,
    const std::string& approval_rationale) {
    ReviewDecision intake;
    intake.id = ReviewDecisionId{intake_decision_id};
    intake.reviewer = ReviewerIdentity{"check.synthetic-intake-v1", ActorKind::automated_check, std::nullopt};
    intake.timestamp = "2026-10-31T10:00:00Z";
    intake.rationale = "Synthetic fixture passed structural intake checks; queued for engineering review.";
    intake.evidence_refs = obs.evidence_refs;
    intake.previous_state = ReviewState::candidate;
    intake.new_state = ReviewState::needs_review;
    apply_review_decision(obs, intake);

    ReviewDecision approve;
    approve.id = ReviewDecisionId{approval_decision_id};
    approve.reviewer = engineer;
    approve.timestamp = "2026-10-31T10:15:00Z";
    approve.rationale = approval_rationale;
    approve.evidence_refs = obs.evidence_refs;
    approve.previous_state = ReviewState::needs_review;
    approve.new_state = ReviewState::approved;
    apply_review_decision(obs, approve);
}

} // namespace

ComponentDomainBundle make_synthetic_resistor_bundle() {
    ComponentDomainBundle bundle;

    bundle.component.id = ComponentId{"comp.synthetic.resistor.syn-tf0603-1k00-lf"};
    bundle.component.manufacturer = "Datasim Synthetic Passive Corp";
    bundle.component.part_number = "SYN-TF0603-1K00-LF";
    bundle.component.part_number_suffix = "LF";
    bundle.component.package_id = "0603";
    bundle.component.family = ComponentFamily::resistor;
    bundle.component.subtype = "Synthetic precision thin-film chip resistor";
    bundle.component.is_synthetic = true;

    SourceDocument doc;
    doc.id = SourceDocumentId{"doc.synthetic.syn-tf0603-spec-rev-a"};
    doc.publisher = "Datasim Synthetic Passive Corp";
    doc.title = "SYN-TF0603 Synthetic Thin-Film Resistor Specification (Domain Test Fixture)";
    doc.associated_part_numbers = {"SYN-TF0603-1K00-LF"};
    doc.revision = "SYN-REV-A";
    doc.original_url = "urn:datasim:synthetic:datasheet:syn-tf0603-rev-a";
    doc.retrieval_date = "2026-10-31";
    doc.content_sha256 = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
    doc.usage_rights = DocumentUsageRights{
        "CC0-1.0-Synthetic-Test-Fixture",
        true,
        "Synthetic fixture authored for Datasim unit testing; represents no real vendor device."};
    doc.is_synthetic = true;
    bundle.documents.push_back(doc);

    // Note: pdf_page_index (0-based) intentionally differs from printed_page_label.
    EvidenceLocation ev_limits;
    ev_limits.id = EvidenceId{"ev.res.table1.limits"};
    ev_limits.source_document_id = doc.id;
    ev_limits.pdf_page_index = 2U;
    ev_limits.printed_page_label = "1";
    ev_limits.section = "2.1 Limiting Values";
    ev_limits.table_or_figure_id = "Table 1";
    ev_limits.cell_or_region = "Row 'Maximum Power Dissipation'";
    ev_limits.footnote_refs = {"Note 1: Linear derating applies above 70 degC ambient."};
    bundle.evidence_locations.push_back(ev_limits);

    EvidenceLocation ev_operating;
    ev_operating.id = EvidenceId{"ev.res.table2.operating"};
    ev_operating.source_document_id = doc.id;
    ev_operating.pdf_page_index = 2U;
    ev_operating.printed_page_label = "1";
    ev_operating.section = "2.2 Recommended Operating Conditions";
    ev_operating.table_or_figure_id = "Table 2";
    ev_operating.cell_or_region = "Row 'Ambient Operating Temperature'";
    bundle.evidence_locations.push_back(ev_operating);

    EvidenceLocation ev_elec;
    ev_elec.id = EvidenceId{"ev.res.table3.electrical"};
    ev_elec.source_document_id = doc.id;
    ev_elec.pdf_page_index = 3U;
    ev_elec.printed_page_label = "2";
    ev_elec.section = "2.3 Electrical and Thermal Characteristics";
    ev_elec.table_or_figure_id = "Table 3";
    ev_elec.cell_or_region = "Rows 'DC Resistance' and 'Temperature Coefficient'";
    ev_elec.footnote_refs = {"Note 2: Low-power four-terminal measurement at T_A = 25 degC."};
    bundle.evidence_locations.push_back(ev_elec);

    const OperatingCondition ta_25{
        "T_A",
        "25",
        "degC",
        25.0,
        "Still-air reference ambient temperature"};
    const OperatingCondition ta_derating_knee{
        "T_A",
        "<= 70",
        "degC",
        std::nullopt,
        "Below derating knee"};
    const OperatingCondition ta_full_span{
        "T_A",
        "-55 to +125",
        "degC",
        std::nullopt,
        "Full rated ambient span"};

    const ReviewerIdentity reviewer{
        "eng.synthetic.reviewer-1",
        ActorKind::human_engineer,
        "Synthetic Lead Component Engineer"};

    auto make_res_obs = [&](const std::string& id,
                            const std::string& name,
                            std::optional<std::string> text,
                            std::optional<std::string> unit,
                            std::optional<double> num,
                            ValueQualifier qual,
                            ParameterCategory cat,
                            std::vector<OperatingCondition> conds,
                            EvidenceId ev) {
        ParameterObservation obs;
        obs.id = ObservationId{id};
        obs.component_id = bundle.component.id;
        obs.parameter_name = name;
        obs.original_value_text = std::move(text);
        obs.original_unit_symbol = std::move(unit);
        obs.parsed_numeric_value = num;
        obs.qualifier = qual;
        obs.category = cat;
        obs.operating_conditions = std::move(conds);
        obs.evidence_refs = {ev};
        obs.extraction_method = ExtractionMethod::synthetic_fixture;
        obs.extraction_tool = "datasim-synthetic-fixture-builder";
        obs.extraction_tool_version = "2.0.0";
        obs.extracted_at = "2026-10-31T09:50:00Z";
        return obs;
    };

    ParameterObservation obs_r_min = make_res_obs(
        "obs.res.r_dc.min",
        "R_DC",
        "0.999",
        "kohm",
        0.999,
        ValueQualifier::minimum,
        ParameterCategory::electrical_characteristic,
        {ta_25},
        ev_elec.id);
    promote_observation_through_review(
        obs_r_min,
        "rev.res.r_min.intake",
        "rev.res.r_min.approve",
        reviewer,
        "Verified synthetic minimum resistance bound at 25 degC.");

    ParameterObservation obs_r_typ = make_res_obs(
        "obs.res.r_dc.typ",
        "R_DC",
        "1.000",
        "kohm",
        1.000,
        ValueQualifier::typical,
        ParameterCategory::electrical_characteristic,
        {ta_25},
        ev_elec.id);
    promote_observation_through_review(
        obs_r_typ,
        "rev.res.r_typ.intake",
        "rev.res.r_typ.approve",
        reviewer,
        "Verified synthetic nominal/typical resistance value at 25 degC.");

    ParameterObservation obs_r_max = make_res_obs(
        "obs.res.r_dc.max",
        "R_DC",
        "1.001",
        "kohm",
        1.001,
        ValueQualifier::maximum,
        ParameterCategory::electrical_characteristic,
        {ta_25},
        ev_elec.id);
    promote_observation_through_review(
        obs_r_max,
        "rev.res.r_max.intake",
        "rev.res.r_max.approve",
        reviewer,
        "Verified synthetic maximum resistance bound at 25 degC.");

    ParameterObservation obs_tcr_max = make_res_obs(
        "obs.res.tcr.max",
        "TCR",
        "+/-25",
        "ppm/K",
        25.0,
        ValueQualifier::maximum,
        ParameterCategory::thermal_characteristic,
        {ta_full_span},
        ev_elec.id);
    promote_observation_through_review(
        obs_tcr_max,
        "rev.res.tcr.intake",
        "rev.res.tcr.approve",
        reviewer,
        "Verified synthetic +/-25 ppm/K magnitude ceiling over -55 to +125 degC.");

    ParameterObservation obs_pd_abs = make_res_obs(
        "obs.res.p_diss.abs_max",
        "P_D_MAX",
        "0.100",
        "W",
        0.100,
        ValueQualifier::maximum,
        ParameterCategory::absolute_maximum_rating,
        {ta_derating_knee},
        ev_limits.id);
    promote_observation_through_review(
        obs_pd_abs,
        "rev.res.pd.intake",
        "rev.res.pd.approve",
        reviewer,
        "Verified synthetic absolute maximum dissipation limit; stress bound only.");

    ParameterObservation obs_ta_rec_max = make_res_obs(
        "obs.res.ta_op.rec_max",
        "T_A_OP",
        "125",
        "degC",
        125.0,
        ValueQualifier::maximum,
        ParameterCategory::recommended_operating_condition,
        {OperatingCondition{"P_load", "<= 0.050", "W", std::nullopt, "Derated continuous load"}},
        ev_operating.id);
    promote_observation_through_review(
        obs_ta_rec_max,
        "rev.res.ta_rec.intake",
        "rev.res.ta_rec.approve",
        reviewer,
        "Verified synthetic recommended upper operating ambient temperature.");

    // Rejected candidate retained in observation history and excluded from ComponentKnowledge.
    ParameterObservation obs_rejected = make_res_obs(
        "obs.res.r_dc.ocr_typo",
        "R_DC",
        "10.00",
        "kohm",
        10.0,
        ValueQualifier::typical,
        ParameterCategory::electrical_characteristic,
        {ta_25},
        ev_elec.id);
    ReviewDecision rej_intake{
        ReviewDecisionId{"rev.res.typo.intake"},
        ReviewerIdentity{"check.synthetic-intake-v1", ActorKind::automated_check, std::nullopt},
        "2026-10-31T10:00:00Z",
        "Queued candidate observation for review.",
        {ev_elec.id},
        ReviewState::candidate,
        ReviewState::needs_review,
        std::nullopt};
    apply_review_decision(obs_rejected, rej_intake);
    ReviewDecision rej_final{
        ReviewDecisionId{"rev.res.typo.reject"},
        reviewer,
        "2026-10-31T10:16:00Z",
        "Synthetic decimal-shift error; contradicts Table 3 1.000 kohm entry.",
        {ev_elec.id},
        ReviewState::needs_review,
        ReviewState::rejected,
        std::nullopt};
    apply_review_decision(obs_rejected, rej_final);

    bundle.observations = {
        obs_r_min,
        obs_r_typ,
        obs_r_max,
        obs_tcr_max,
        obs_pd_abs,
        obs_ta_rec_max,
        obs_rejected};

    ComponentKnowledge kn;
    kn.id = ComponentKnowledgeId{"ck.synthetic.syn-tf0603-1k00-lf.v1"};
    kn.component = bundle.component;
    kn.version = 1U;
    kn.previous_version_id = std::nullopt;
    kn.review_decision = ReviewDecision{
        ReviewDecisionId{"rev.res.knowledge.v1"},
        reviewer,
        "2026-10-31T10:30:00Z",
        "Approved synthetic resistor knowledge v1 from reviewed Table 1-3 observations.",
        {ev_limits.id, ev_operating.id, ev_elec.id},
        ReviewState::needs_review,
        ReviewState::approved,
        std::nullopt};
    kn.version_history.push_back(KnowledgeRevisionEntry{
        1U,
        kn.id,
        std::nullopt,
        reviewer,
        "2026-10-31T10:30:00Z",
        "Initial synthetic resistor knowledge promotion."});

    promote_parameter_to_knowledge(
        kn,
        obs_r_min,
        make_normalized_parameter(
            NormalizedParameterId{"np.res.r_dc.min"},
            obs_r_min,
            "dc_resistance",
            "Minimum DC electrical resistance at 25 degC",
            999.0,
            "ohm",
            false,
            "explicit_scale: 0.999 kohm * 1000 = 999.0 ohm",
            obs_r_min.operating_conditions));

    promote_parameter_to_knowledge(
        kn,
        obs_r_typ,
        make_normalized_parameter(
            NormalizedParameterId{"np.res.r_dc.typ"},
            obs_r_typ,
            "dc_resistance",
            "Nominal/typical DC electrical resistance at 25 degC",
            1000.0,
            "ohm",
            false,
            "explicit_scale: 1.000 kohm * 1000 = 1000.0 ohm",
            obs_r_typ.operating_conditions));

    promote_parameter_to_knowledge(
        kn,
        obs_r_max,
        make_normalized_parameter(
            NormalizedParameterId{"np.res.r_dc.max"},
            obs_r_max,
            "dc_resistance",
            "Maximum DC electrical resistance at 25 degC",
            1001.0,
            "ohm",
            false,
            "explicit_scale: 1.001 kohm * 1000 = 1001.0 ohm",
            obs_r_max.operating_conditions));

    promote_parameter_to_knowledge(
        kn,
        obs_tcr_max,
        make_normalized_parameter(
            NormalizedParameterId{"np.res.tcr.max"},
            obs_tcr_max,
            "temperature_coefficient_of_resistance",
            "Maximum absolute temperature coefficient of resistance",
            2.5e-5,
            "1/K",
            false,
            "explicit_scale: 25 ppm/K * 1e-6 = 2.5e-5 1/K (magnitude ceiling of +/-25 ppm/K)",
            obs_tcr_max.operating_conditions));

    promote_parameter_to_knowledge(
        kn,
        obs_pd_abs,
        make_normalized_parameter(
            NormalizedParameterId{"np.res.p_diss.abs_max"},
            obs_pd_abs,
            "max_power_dissipation_limit",
            "Absolute maximum continuous power dissipation stress limit",
            0.100,
            "W",
            false,
            "identity: 0.100 W = 0.100 W",
            obs_pd_abs.operating_conditions));

    promote_parameter_to_knowledge(
        kn,
        obs_ta_rec_max,
        make_normalized_parameter(
            NormalizedParameterId{"np.res.ta_op.rec_max"},
            obs_ta_rec_max,
            "recommended_ambient_temperature",
            "Upper recommended operating ambient temperature",
            398.15,
            "K",
            false,
            "explicit_offset: 125 degC + 273.15 = 398.15 K",
            obs_ta_rec_max.operating_conditions));

    bundle.knowledge = kn;

    ModelArtifactDescriptor model;
    model.id = ModelArtifactId{"model.synthetic.resistor.ohmic-linear"};
    model.component_id = bundle.component.id;
    model.component_knowledge_id = kn.id;
    model.version = "1.0.0";
    model.format = ModelFormat::analytical_equation;
    model.source_vendor = "Datasim Synthetic Model Catalog";
    model.artifact_uri = "urn:datasim:synthetic:equation:linear-ohmic-resistor:1.0.0";
    model.artifact_sha256 = std::nullopt;
    model.license = ModelLicenseMetadata{
        "CC0-1.0-Synthetic-Test-Fixture",
        true,
        "Synthetic model metadata contract for unit testing only."};
    model.terminals = {
        TerminalDescriptor{1U, "p1", "1", "First non-polarized resistor terminal"},
        TerminalDescriptor{2U, "p2", "2", "Second non-polarized resistor terminal"}};
    model.parameter_mappings = {
        ModelParameterMapping{
            "R",
            NormalizedParameterId{"np.res.r_dc.typ"},
            std::nullopt,
            "ohm",
            "V(p1,p2) = R * I(p1,p2)"}};
    model.validity_envelope = ModelValidityEnvelope{
        {NumericRangeConstraint{"ambient_temperature", -55.0, 125.0, "degC", true, true},
         NumericRangeConstraint{"signal_frequency", 0.0, 1.0e6, "Hz", true, true},
         NumericRangeConstraint{"continuous_power", 0.0, 0.050, "W", true, true}},
        {"Ideal linear time-invariant ohmic conduction at reference temperature."},
        "Excludes high-frequency parasitic inductance/capacitance and self-heating."};
    model.supported_analyses = {
        AnalysisType::dc_operating_point,
        AnalysisType::dc_sweep,
        AnalysisType::ac_small_signal,
        AnalysisType::transient};
    model.limitations = {
        "Does not model temperature-dependent resistance drift unless paired with thermal coupling.",
        "Does not model package parasitic series inductance or shunt capacitance.",
        "Does not enforce absolute maximum power destruction; stress limits remain separate checks."};
    model.validation_record_refs = {ModelValidationId{"mval.synthetic.resistor.initial"}};
    model.is_synthetic = true;
    bundle.models.push_back(model);

    // Intentionally unvalidated: demonstrates that approved observations + approved knowledge
    // + valid model descriptor metadata do not automatically validate the model.
    ModelValidationRecord mval;
    mval.id = ModelValidationId{"mval.synthetic.resistor.initial"};
    mval.model_id = model.id;
    mval.model_version = model.version;
    mval.status = ModelValidationStatus::unvalidated;
    mval.validation_kind = ModelValidationKind::none;
    mval.summary = "Registered metadata contract only; no numerical or physical model validation executed.";
    bundle.model_validations.push_back(mval);

    return bundle;
}

ComponentDomainBundle make_synthetic_capacitor_bundle() {
    ComponentDomainBundle bundle;

    bundle.component.id = ComponentId{"comp.synthetic.capacitor.syn-c0g0805-1u0-j50nt"};
    bundle.component.manufacturer = "Datasim Synthetic Dielectric Works";
    bundle.component.part_number = "SYN-C0G0805-1U0-J50NT";
    bundle.component.part_number_suffix = "J50NT";
    bundle.component.package_id = "0805";
    bundle.component.family = ComponentFamily::capacitor;
    bundle.component.subtype = "Synthetic Class-I C0G multilayer ceramic chip capacitor";
    bundle.component.is_synthetic = true;

    SourceDocument doc;
    doc.id = SourceDocumentId{"doc.synthetic.syn-c0g0805-spec-2026"};
    doc.publisher = "Datasim Synthetic Dielectric Works";
    doc.title = "SYN-C0G0805 Synthetic Ceramic Capacitor Engineering Data (Domain Test Fixture)";
    doc.associated_part_numbers = {"SYN-C0G0805-1U0-J50NT"};
    doc.revision = "SYN-2026.10";
    doc.original_url = "urn:datasim:synthetic:datasheet:syn-c0g0805-2026-10";
    doc.retrieval_date = "2026-10-31";
    doc.content_sha256 = "fedcba9876543210fedcba9876543210fedcba9876543210fedcba9876543210";
    doc.usage_rights = DocumentUsageRights{
        "CC0-1.0-Synthetic-Test-Fixture",
        true,
        "Synthetic capacitor fixture for Datasim Phase 2 domain unit tests only."};
    doc.is_synthetic = true;
    bundle.documents.push_back(doc);

    EvidenceLocation ev_ratings;
    ev_ratings.id = EvidenceId{"ev.cap.table1.ratings"};
    ev_ratings.source_document_id = doc.id;
    ev_ratings.pdf_page_index = 4U;
    ev_ratings.printed_page_label = "A-1";
    ev_ratings.section = "3.1 Voltage Ratings";
    ev_ratings.table_or_figure_id = "Table A1";
    ev_ratings.cell_or_region = "Rows 'Rated DC Voltage' and 'Dielectric Withstand Voltage'";
    ev_ratings.footnote_refs = {"Footnote a: Surge test duration <= 5 s, charge/discharge current <= 50 mA."};
    bundle.evidence_locations.push_back(ev_ratings);

    EvidenceLocation ev_cap_table;
    ev_cap_table.id = EvidenceId{"ev.cap.table2.characteristics"};
    ev_cap_table.source_document_id = doc.id;
    ev_cap_table.pdf_page_index = 5U;
    ev_cap_table.printed_page_label = "A-2";
    ev_cap_table.section = "3.2 Electrical Characteristics";
    ev_cap_table.table_or_figure_id = "Table A2";
    ev_cap_table.cell_or_region = "Rows 'Capacitance', 'Dissipation Factor', 'Insulation Current'";
    bundle.evidence_locations.push_back(ev_cap_table);

    const std::vector<OperatingCondition> cap_test_conds = {
        OperatingCondition{"T_A", "25", "degC", 25.0, std::nullopt},
        OperatingCondition{"f_test", "1.0", "kHz", 1.0, std::nullopt},
        OperatingCondition{"V_ac_rms", "1.0", "V", 1.0, "Sinusoidal small-signal bridge drive"}};

    const ReviewerIdentity reviewer{
        "eng.synthetic.reviewer-2",
        ActorKind::human_engineer,
        "Synthetic Passive Qualification Reviewer"};

    auto make_cap_obs = [&](const std::string& id,
                            const std::string& name,
                            std::optional<std::string> text,
                            std::optional<std::string> unit,
                            bool unitless,
                            std::optional<double> num,
                            ValueQualifier qual,
                            ParameterCategory cat,
                            std::vector<OperatingCondition> conds,
                            EvidenceId ev) {
        ParameterObservation obs;
        obs.id = ObservationId{id};
        obs.component_id = bundle.component.id;
        obs.parameter_name = name;
        obs.original_value_text = std::move(text);
        obs.original_unit_symbol = std::move(unit);
        obs.unitless = unitless;
        obs.parsed_numeric_value = num;
        obs.qualifier = qual;
        obs.category = cat;
        obs.operating_conditions = std::move(conds);
        obs.evidence_refs = {ev};
        obs.extraction_method = ExtractionMethod::synthetic_fixture;
        obs.extraction_tool = "datasim-synthetic-fixture-builder";
        obs.extraction_tool_version = "2.0.0";
        obs.extracted_at = "2026-10-31T11:00:00Z";
        return obs;
    };

    // Earlier observation superseded after corrected tolerance reading.
    ParameterObservation obs_c_typ_old = make_cap_obs(
        "obs.cap.c.typ.rev0",
        "C",
        "1.0",
        "uF",
        false,
        1.0,
        ValueQualifier::typical,
        ParameterCategory::electrical_characteristic,
        cap_test_conds,
        ev_cap_table.id);
    promote_observation_through_review(
        obs_c_typ_old,
        "rev.cap.c_old.intake",
        "rev.cap.c_old.approve",
        reviewer,
        "Initial synthetic capacitance reading.");

    ParameterObservation obs_c_min = make_cap_obs(
        "obs.cap.c.min",
        "C",
        "0.950",
        "uF",
        false,
        0.950,
        ValueQualifier::minimum,
        ParameterCategory::electrical_characteristic,
        cap_test_conds,
        ev_cap_table.id);
    promote_observation_through_review(
        obs_c_min,
        "rev.cap.c_min.intake",
        "rev.cap.c_min.approve",
        reviewer,
        "Verified synthetic -5% J-tolerance lower bound.");

    ParameterObservation obs_c_typ = make_cap_obs(
        "obs.cap.c.typ",
        "C",
        "1.000",
        "uF",
        false,
        1.000,
        ValueQualifier::typical,
        ParameterCategory::electrical_characteristic,
        cap_test_conds,
        ev_cap_table.id);
    promote_observation_through_review(
        obs_c_typ,
        "rev.cap.c_typ.intake",
        "rev.cap.c_typ.approve",
        reviewer,
        "Verified synthetic three-significant-digit capacitance reading.");

    // Transition obs_c_typ_old -> superseded by obs_c_typ.
    ReviewDecision supersede_old{
        ReviewDecisionId{"rev.cap.c_old.supersede"},
        reviewer,
        "2026-10-31T11:20:00Z",
        "Superseded by obs.cap.c.typ preserving exact 1.000 uF significant digits.",
        {ev_cap_table.id},
        ReviewState::approved,
        ReviewState::superseded,
        obs_c_typ.id};
    apply_review_decision(obs_c_typ_old, supersede_old);

    ParameterObservation obs_c_max = make_cap_obs(
        "obs.cap.c.max",
        "C",
        "1.050",
        "uF",
        false,
        1.050,
        ValueQualifier::maximum,
        ParameterCategory::electrical_characteristic,
        cap_test_conds,
        ev_cap_table.id);
    promote_observation_through_review(
        obs_c_max,
        "rev.cap.c_max.intake",
        "rev.cap.c_max.approve",
        reviewer,
        "Verified synthetic +5% J-tolerance upper bound.");

    ParameterObservation obs_df_max = make_cap_obs(
        "obs.cap.tan_delta.max",
        "tan_delta",
        "<= 0.0015",
        std::nullopt,
        true,
        0.0015,
        ValueQualifier::maximum,
        ParameterCategory::electrical_characteristic,
        cap_test_conds,
        ev_cap_table.id);
    promote_observation_through_review(
        obs_df_max,
        "rev.cap.df.intake",
        "rev.cap.df.approve",
        reviewer,
        "Verified dimensionless dissipation factor upper limit preserving inequality notation.");

    ParameterObservation obs_v_rec = make_cap_obs(
        "obs.cap.v_rated.rec_max",
        "V_RATED_DC",
        "50",
        "V",
        false,
        50.0,
        ValueQualifier::maximum,
        ParameterCategory::recommended_operating_condition,
        {OperatingCondition{"T_A", "-55 to +125", "degC", std::nullopt, std::nullopt}},
        ev_ratings.id);
    promote_observation_through_review(
        obs_v_rec,
        "rev.cap.vrec.intake",
        "rev.cap.vrec.approve",
        reviewer,
        "Verified continuous rated DC operating voltage.");

    ParameterObservation obs_v_abs = make_cap_obs(
        "obs.cap.v_withstand.abs_max",
        "V_DWV",
        "125",
        "V",
        false,
        125.0,
        ValueQualifier::maximum,
        ParameterCategory::absolute_maximum_rating,
        {OperatingCondition{"t_dwell", "<= 5", "s", std::nullopt, "Short proof test only"}},
        ev_ratings.id);
    promote_observation_through_review(
        obs_v_abs,
        "rev.cap.vabs.intake",
        "rev.cap.vabs.approve",
        reviewer,
        "Verified short-duration dielectric withstand stress limit; never continuous operating voltage.");

    // Blank datasheet cell: missing value remains missing (std::nullopt, not 0.0) and unpromoted.
    ParameterObservation obs_missing_cell = make_cap_obs(
        "obs.cap.ileak.typ_blank",
        "I_LEAK",
        std::nullopt,
        "nA",
        false,
        std::nullopt,
        ValueQualifier::typical,
        ParameterCategory::electrical_characteristic,
        {OperatingCondition{"V_DC", "50", "V", 50.0, std::nullopt}},
        ev_cap_table.id);

    bundle.observations = {
        obs_c_typ_old,
        obs_c_min,
        obs_c_typ,
        obs_c_max,
        obs_df_max,
        obs_v_rec,
        obs_v_abs,
        obs_missing_cell};

    ComponentKnowledge kn;
    kn.id = ComponentKnowledgeId{"ck.synthetic.syn-c0g0805-1u0-j50nt.v1"};
    kn.component = bundle.component;
    kn.version = 1U;
    kn.previous_version_id = std::nullopt;
    kn.review_decision = ReviewDecision{
        ReviewDecisionId{"rev.cap.knowledge.v1"},
        reviewer,
        "2026-10-31T11:30:00Z",
        "Approved synthetic capacitor knowledge v1 excluding superseded and blank-cell observations.",
        {ev_ratings.id, ev_cap_table.id},
        ReviewState::needs_review,
        ReviewState::approved,
        std::nullopt};
    kn.version_history.push_back(KnowledgeRevisionEntry{
        1U,
        kn.id,
        std::nullopt,
        reviewer,
        "2026-10-31T11:30:00Z",
        "Initial synthetic capacitor knowledge promotion."});

    promote_parameter_to_knowledge(
        kn,
        obs_c_min,
        make_normalized_parameter(
            NormalizedParameterId{"np.cap.c.min"},
            obs_c_min,
            "capacitance",
            "Minimum small-signal capacitance at 25 degC, 1 kHz, 1 Vrms",
            0.950e-6,
            "F",
            false,
            "explicit_scale: 0.950 uF * 1e-6 = 9.5e-7 F",
            obs_c_min.operating_conditions));

    promote_parameter_to_knowledge(
        kn,
        obs_c_typ,
        make_normalized_parameter(
            NormalizedParameterId{"np.cap.c.typ"},
            obs_c_typ,
            "capacitance",
            "Nominal/typical small-signal capacitance at 25 degC, 1 kHz, 1 Vrms",
            1.000e-6,
            "F",
            false,
            "explicit_scale: 1.000 uF * 1e-6 = 1.0e-6 F",
            obs_c_typ.operating_conditions));

    promote_parameter_to_knowledge(
        kn,
        obs_c_max,
        make_normalized_parameter(
            NormalizedParameterId{"np.cap.c.max"},
            obs_c_max,
            "capacitance",
            "Maximum small-signal capacitance at 25 degC, 1 kHz, 1 Vrms",
            1.050e-6,
            "F",
            false,
            "explicit_scale: 1.050 uF * 1e-6 = 1.05e-6 F",
            obs_c_max.operating_conditions));

    promote_parameter_to_knowledge(
        kn,
        obs_df_max,
        make_normalized_parameter(
            NormalizedParameterId{"np.cap.tan_delta.max"},
            obs_df_max,
            "dissipation_factor",
            "Maximum dimensionless dielectric loss tangent at 1 kHz",
            0.0015,
            std::nullopt,
            true,
            "identity_dimensionless: <= 0.0015 upper bound retained as 0.0015",
            obs_df_max.operating_conditions));

    promote_parameter_to_knowledge(
        kn,
        obs_v_rec,
        make_normalized_parameter(
            NormalizedParameterId{"np.cap.v_rated.rec_max"},
            obs_v_rec,
            "rated_dc_voltage",
            "Maximum recommended continuous DC terminal voltage",
            50.0,
            "V",
            false,
            "identity: 50 V = 50.0 V",
            obs_v_rec.operating_conditions));

    promote_parameter_to_knowledge(
        kn,
        obs_v_abs,
        make_normalized_parameter(
            NormalizedParameterId{"np.cap.v_withstand.abs_max"},
            obs_v_abs,
            "dielectric_withstand_voltage_limit",
            "Short-duration dielectric proof stress limit",
            125.0,
            "V",
            false,
            "identity: 125 V = 125.0 V",
            obs_v_abs.operating_conditions));

    bundle.knowledge = kn;

    ModelArtifactDescriptor model;
    model.id = ModelArtifactId{"model.synthetic.capacitor.c0g-subckt"};
    model.component_id = bundle.component.id;
    model.component_knowledge_id = kn.id;
    model.version = "1.0.0";
    model.format = ModelFormat::spice_subcircuit;
    model.source_vendor = "Datasim Synthetic Model Catalog";
    model.artifact_uri = "urn:datasim:synthetic:subcircuit:syn-c0g0805-1u0:1.0.0";
    model.artifact_sha256 = "abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789";
    model.license = ModelLicenseMetadata{
        "CC0-1.0-Synthetic-Test-Fixture",
        true,
        "Synthetic subcircuit descriptor metadata only; no external model loader or execution in Phase 2."};
    model.terminals = {
        TerminalDescriptor{1U, "port_a", "1", "First symmetric capacitor electrode"},
        TerminalDescriptor{2U, "port_b", "2", "Second symmetric capacitor electrode"}};
    model.parameter_mappings = {
        ModelParameterMapping{
            "C_NOM",
            NormalizedParameterId{"np.cap.c.typ"},
            std::nullopt,
            "F",
            "C_NOM = capacitance(typical)"},
        ModelParameterMapping{
            "ESR_SYNTH",
            std::nullopt,
            0.050,
            "ohm",
            "Fixed synthetic placeholder series resistance metadata"}};
    model.validity_envelope = ModelValidityEnvelope{
        {NumericRangeConstraint{"ambient_temperature", -55.0, 125.0, "degC", true, true},
         NumericRangeConstraint{"terminal_dc_voltage", -50.0, 50.0, "V", true, true},
         NumericRangeConstraint{"signal_frequency", 10.0, 1.0e7, "Hz", true, true}},
        {"Small-signal linear capacitance within recommended 50 V DC envelope."},
        "Metadata contract only; subcircuit text is neither loaded nor simulated in Phase 2."};
    model.supported_analyses = {
        AnalysisType::dc_operating_point,
        AnalysisType::ac_small_signal,
        AnalysisType::transient};
    model.limitations = {
        "Does not represent dielectric breakdown above 50 V rated operating voltage.",
        "Does not capture board-mount pad inductance or high-frequency self-resonance harmonics."};
    model.validation_record_refs = {};
    model.is_synthetic = true;
    bundle.models.push_back(model);

    return bundle;
}

} // namespace datasim::component::testing
