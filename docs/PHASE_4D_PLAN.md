# Phase 4D plan: PDF-to-simulation end-to-end validation gate

Status: **IMPLEMENTED, MERGED into `main` via PR #8 (`69aba4df7ff752355a40e394de5d05ac988df34b`),
and VERIFIED on Windows x64 (MSYS2 UCRT64 GCC 16.2.0, Poppler 26.10.0, ngspice-47)
locally and in hosted Windows CI runs
[42023680012](https://github.com/quocthaile/datasim/actions/runs/42023680012) (PR #8)
and [42025406013](https://github.com/quocthaile/datasim/actions/runs/42025406013)
(post-merge `main`).** No real-datasheet extraction accuracy or physical measurement
(`MEASURED`) is claimed in Phase 4D (see [Phase 4E validation](PHASE_4E_VALIDATION.md)
for the subsequent offline real-datasheet pilot benchmark).

## 1. Objective and baseline

Phase 4C merged into `main` via PR #7 at commit
`1704f726d901f3e1d00f9cdc918e80d352b443ac`, with post-merge hosted Windows CI run
[42017272043](https://github.com/quocthaile/datasim/actions/runs/42017272043)
passing all build, unit, Poppler extraction, real `ngspice-47` integration, and
convergence/repeatability steps.

Phase 4C verified `datasim_pdf` through `datasim_intake` (`build_candidate_bundle()`),
while Phase 4B verified hand-constructed in-memory `IntakeManifest` fixtures through
`datasim_vertical` and `ngspice`. Phase 4D closes the remaining integration gap by
connecting the real Poppler-backed `datasim_pdf::extract_electrical_table()` path on
the fictional selectable-text PDF fixture (`tests/fixtures/synthetic_datasheet.pdf`)
all the way through intake validation, candidate-only enforcement, synthetic human
engineering review, conflict and ambiguity handling, explicit SI normalization,
`ComponentKnowledge` promotion, independent model validation, `CircuitAssembly`
construction, `datasim_vertical::execute_vertical_rc_run()`, real `ngspice`
simulation, numerical/byte-level equivalence against an equivalent manual `RcConfig`,
and provenance verification.

This is a narrowly scoped integration and validation gate, not an architecture
refactor. The five libraries (`datasim`, `datasim_component`, `datasim_vertical`,
`datasim_intake`, `datasim_pdf`) keep their existing dependency graph and public
interfaces.

## 2. Existing contracts reused

Phase 4D introduces no new public library APIs and reuses these exact signatures:

- **PDF table extraction (`src/datasim/pdf/table_extraction.hpp`)**:
  - `datasim::pdf::ExtractionRequest`, `ElectricalTableLayout`, `HorizontalSpan`, `ExtractionResult`
  - `datasim::pdf::extract_electrical_table(const ExtractionRequest& request)`
- **Evidence intake boundary (`src/datasim/intake/intake.hpp`)**:
  - `datasim::intake::IntakeManifest`, `IntakeOriginClass`, `IntakeReport`, `IntakeDiagnosticCode`
  - `datasim::intake::validate_intake_manifest(const IntakeManifest& manifest)`
  - `datasim::intake::build_candidate_bundle(const IntakeManifest& manifest)`
  - `datasim::intake::parse_unambiguous_scalar(std::string_view text)`
- **Component domain model (`src/datasim/component/...`)**:
  - `datasim::component::apply_review_decision(ParameterObservation& observation, const ReviewDecision& decision)`
  - `datasim::component::make_normalized_parameter(...)`
  - `datasim::component::promote_parameter_to_knowledge(ComponentKnowledge& knowledge, const ParameterObservation& observation, NormalizedParameter parameter)`
  - `datasim::component::ParameterConflict`, `ConflictResolutionStatus`
  - `datasim::component::ModelArtifactDescriptor`, `ModelValidityEnvelope`, `ModelValidationRecord`, `effective_model_validation_status(...)`
  - `datasim::component::validate_domain_bundle(const ComponentDomainBundle& bundle)`
- **Vertical slice & simulation (`src/datasim/vertical/...`, `src/datasim/simulation/...`)**:
  - `datasim::vertical::CircuitAssembly`, `validate_circuit_assembly(const CircuitAssembly& assembly)`
  - `datasim::vertical::resolve_rc_parameters(const CircuitAssembly& assembly)`
  - `datasim::vertical::evaluate_rc_model_compatibility(...)`
  - `datasim::vertical::adapt_vertical_rc_simulation(...)`, `build_provenance_manifest(...)`, `provenance_json(...)`
  - `datasim::vertical::execute_vertical_rc_run(const CircuitAssembly& assembly, const RcRunInputSpecification& run_inputs, const std::string& executable, const std::filesystem::path& new_directory)`
  - `datasim::RcConfig`, `datasim::execute_rc_run(...)`, `datasim::validate(...)`, `datasim::validate_on_grid(...)`

### Synthetic classification alignment for `deterministic_table_parser`

In Phase 3 (`src/datasim/vertical/compatibility/model_compatibility.cpp`),
`inspect_component_synthetic_markers` treated any `ParameterObservation` whose
`extraction_method != ExtractionMethod::synthetic_fixture` as non-synthetic metadata
(`any_non_synthetic = true`). When Phase 4C introduced `datasim_pdf`, extracted
observations received `ExtractionMethod::deterministic_table_parser` even when
extracted from a synthetic PDF fixture (`ComponentIdentity::is_synthetic == true`
and `SourceDocument::is_synthetic == true`). In Phase 4D, `inspect_component_synthetic_markers`
recognizes `ExtractionMethod::deterministic_table_parser` observations whose bundle
component and source documents are all marked `is_synthetic == true` as synthetic
test provenance (`any_synthetic = true`), preserving:

1. `ExtractionMethod::deterministic_table_parser` and `"Poppler pdftotext -bbox-layout"` on PDF-extracted observations.
2. Fail-closed rejection under `ExecutionPolicy::engineering_approved_only` (`VerticalValidationCode::synthetic_data_prohibited_in_engineering_mode`).
3. Fail-closed rejection of mixed synthetic/non-synthetic component, document, observation, or model metadata (`VerticalValidationCode::inconsistent_synthetic_classification`).
4. Acceptance under `ExecutionPolicy::test_only_synthetic` with `ExecutionClassification::test_only_synthetic`.

## 3. Fixture selection, values, units, conditions, and model assumptions

The existing two-page fictional selectable-text PDF fixture
(`tests/fixtures/synthetic_datasheet.pdf`, generated deterministically by
`tests/fixtures/make_synthetic_datasheet_pdf.py`) is reused without modification:

1. **Page 0 (`printed_page_label = "5"`, `table_id = "TABLE_R"`, resistor `SIM-R-10K`)**:
   - Row 1 (`r1`): `Resistance`, `kohm`, `Min = 9.9`, `Typ = 10.0`, `Max = 10.1`, condition `T_A=25 degC; V_DD=5 V`.
   - Row 2 (`r2`): `Resistance`, `kohm`, `Min = 9.8`, `Typ = 10.0`, `Max = 10.2`, condition `T_A=85 degC`.
   - Row 3 (`r3`): `Resistance`, `kohm`, `Min = 9.9`, `Typ = 10.01`, `Max = 10.1`, condition `T_A=25 degC; V_DD=5 V` (conflicts with Row 1 `Typ` at `25 degC`, triggering `IntakeDiagnosticCode::conflict_candidate`).
2. **Page 1 (`printed_page_label = "Appendix C-2"`, `table_id = "TABLE_C"`, capacitor `SIM-C-1UF`)**:
   - Row 1 (`r1`): `Capacitance`, `uF`, `Min = -` (absent), `Typ = 1,000` (ambiguous comma separator, `parsed_numeric_value = std::nullopt`), `Max = 1.1`, condition `T_A=25 degC`.
   - Row 2 (`r2`): `Capacitance`, `uF`, `Min = 0.9`, `Typ = 1.0*` (footnote marker `*`, `parsed_numeric_value = std::nullopt`), `Max = 1.1`, condition `T_A=85 degC`.
   - Row 3 (`r3`): `Capacitance`, `uF`, `Min = 0.9`, `Typ = 1.0`, `Max = 1.1`, condition `T_A=85 degC`.
   - Row 4 (`r4`): `Capacitance`, blank unit (`original_unit_symbol = std::nullopt`, `parsed_numeric_value = std::nullopt`), `Min = 0.8`, `Typ = 0.9`, `Max = 1.0`, condition `T_A=85 degC`.

### Condition-compatible selection and conflict/rejection preservation

- Across Page 0 and Page 1, the only unambiguous, condition-compatible nominal (`typical`) characterization rows are at `T_A=85 degC`:
  - Resistor Row 2 (`r2.min = 9.8 kohm`, `r2.typ = 10.0 kohm`, `r2.max = 10.2 kohm` at `T_A=85 degC`).
  - Capacitor Row 3 (`r3.min = 0.9 uF`, `r3.typ = 1.0 uF`, `r3.max = 1.1 uF` at `T_A=85 degC`).
- On the resistor bundle:
  - The competing `25 degC` typical rows (`r1.typ = 10.0 kohm` vs `r3.typ = 10.01 kohm`) are not silently dropped or averaged: `r1.typ` is reviewed and approved, `r3.typ` is reviewed and rejected with a documented rationale, and an explicit `ParameterConflict` (`conflict.pdf.resistor.r_dc.25c`, `ConflictResolutionStatus::resolved_by_review`, `selected_observation_id = obs.doc.synthetic.rc.TABLE_R.p0.r1.typ`) is recorded in `ComponentKnowledge::conflicts`.
  - All 9 observations remain in `resistor_bundle.observations`.
  - The `T_A=85 degC` observations (`r2.min`, `r2.typ`, `r2.max`) are reviewed (`candidate -> needs_review -> approved`) and normalized into `ComponentKnowledge` (`9800.0 ohm`, `10000.0 ohm`, `10200.0 ohm`) using explicit documented scaling rules (`explicit_scale: 10.0 kohm * 1000 = 10000.0 ohm`).
- On the capacitor bundle:
  - The ambiguous and incomplete typical observations (`r1.typ` `"1,000"`, `r2.typ` `"1.0*"`, `r4.typ` `"0.9"` with blank unit) are transitioned `candidate -> needs_review -> rejected` with explicit human-review rationales and remain visible in `capacitor_bundle.observations` alongside all 11 extracted observations.
  - The unambiguous `T_A=85 degC` observations (`r3.min`, `r3.typ`, `r3.max`) are reviewed (`candidate -> needs_review -> approved`) and normalized into `ComponentKnowledge` (`9.0e-7 F`, `1.0e-6 F`, `1.1e-6 F`) using explicit documented scaling rules (`explicit_scale: 1.0 uF * 1e-6 = 1.0e-6 F`).

### Synthetic model assumptions and simulation configuration

- **Resistor model (`model.pdf.synthetic.sim-r-10k.ohmic-linear`, v1.0.0)**:
  - `ModelFormat::analytical_equation`, `is_synthetic = true`, terminals `p1` (pin `1`) and `p2` (pin `2`), mapping `"R"` to `np.pdf.resistor.r_dc.typ` (`10000.0 ohm`) via `"V(p1,p2) = R * I(p1,p2)"`.
  - Validity envelope: `ambient_temperature` in `[-55.0, 125.0] degC`, `signal_frequency` in `[0.0, 1.0e6] Hz`, `continuous_power` in `[0.0, 0.050] W` (peak power at `5 V` across `10000 ohm` is `2.5 mW <= 50 mW`).
  - Independent `ModelValidationRecord` (`mval.pdf.synthetic.sim-r-10k.validated.v1`): `validated_within_envelope`, `analytical_comparison`, synthetic `human_engineer` validator, citing extracted PDF evidence `ev.doc.synthetic.rc.TABLE_R.p0.r2.typ`.
- **Capacitor model (`model.pdf.synthetic.sim-c-1uf.ideal-linear`, v1.0.0)**:
  - `ModelFormat::analytical_equation`, `is_synthetic = true`, terminals `port_a` (pin `1`) and `port_b` (pin `2`), mapping `"C"` to `np.pdf.capacitor.c.typ` (`1.0e-6 F`) via `"I(port_a,port_b) = C * dV(port_a,port_b)/dt"`.
  - Validity envelope: `ambient_temperature` in `[-55.0, 125.0] degC`, `terminal_dc_voltage` in `[-50.0, 50.0] V`, `signal_frequency` in `[0.0, 1.0e7] Hz`.
  - Independent `ModelValidationRecord` (`mval.pdf.synthetic.sim-c-1uf.validated.v1`): `validated_within_envelope`, `analytical_comparison`, synthetic `human_engineer` validator, citing extracted PDF evidence `ev.doc.synthetic.cap.TABLE_C.p1.r3.typ`.
- **Simulation comparison (`R = 10000.0 ohm`, `C = 1.0e-6 F`, $\tau = 10\,\text{ms}$)**:
  - Default `RcRunInputSpecification` (`voltage_V = 5.0`, `initial_voltage_V = 0.0`, `delay_s = 1e-3`, `rise_s = 1e-9`, `fall_s = 1e-9`, `width_s = 10e-3`, `period_s = 20e-3`, `step_s = 10e-6`, `duration_s = 6e-3`, `trap`, `reltol = 1e-6`, `abstol = 1e-12`, `vntol = 1e-9`, `temp = 27 degC`, `tnom = 27 degC`, `signal_frequency = 50 Hz`, `ExecutionPolicy::test_only_synthetic`).
  - Compared against an equivalent manual `RcConfig` (`resistance_ohm = 10000.0`, `capacitance_F = 1.0e-6`, identical stimulus/grid/solver settings) over all native samples and the 4001-point common validation grid (`tolerance_V = 0.05 V`) plus byte-identical artifact comparison (`results.csv`, `errors.csv`, `circuit.cir`, `validation_report.json`).

## 4. Negative and deterministic test matrix

All negative cases must fail closed without producing an approved model or a
successful simulation run:

| Case | Scenario | Expected fail-closed behavior |
| --- | --- | --- |
| G-01 | End-to-end PDF extraction -> intake -> review -> knowledge -> model -> `datasim_vertical` -> `ngspice` | Succeeds under `ExecutionPolicy::test_only_synthetic`; 4001 common-grid points within `0.05 V`; byte-identical `results.csv`, `errors.csv`, `circuit.cir`, `validation_report.json` against manual `RcConfig(10000.0, 1.0e-6)`; complete `provenance_metadata.json`. |
| N-01 | Incorrect exact MPN (`SIM-R-10K-WRONG`), suffix mismatch, or publisher mismatch | `extract_electrical_table` / `validate_intake_manifest` fails (`incompatible_source_document_part_number`, `part_number_suffix_mismatch`, `manufacturer_publisher_mismatch`); `candidate_bundle == std::nullopt`. |
| N-02 | Invalid PDF page index (`pdf_page_index = 2` on 2-page PDF) | `extract_electrical_table` fails closed with out-of-range diagnostic; `candidate_bundle == std::nullopt`. |
| N-03 | Missing selectable PDF text (`image_only_datasheet.pdf` or empty table region) | `extract_electrical_table` fails closed with `"no selectable text"` diagnostic; `candidate_bundle == std::nullopt`. |
| N-04 | Ambiguous numeric value (`"1,000"`, `"1.0*"`, blank unit) | `parsed_numeric_value == std::nullopt` with warning diagnostic; asserted numeric parse fails intake (`asserted_ambiguous_numeric_value`); unparsed value fails parameter resolution (`missing_or_invalid_si_value`) and blocks simulation. |
| N-05 | Conflicting observations (`10.0` vs `10.01 kohm` at `25 degC`) without documented resolution | Approving both without `ParameterConflict` fails `validate_domain_bundle` (`undeclared_parameter_conflict`); unresolved conflict blocks `promote_parameter_to_knowledge` and fails `validate_circuit_assembly` (`unresolved_knowledge_conflicts`). |
| N-06 | Candidate promoted without required review transitions | Direct `candidate -> approved` rejected (`invalid_review_transition`); `make_normalized_parameter` / `promote_parameter_to_knowledge` throw `std::invalid_argument` on unapproved observations; unapproved knowledge fails `validate_circuit_assembly` / `execute_vertical_rc_run`. |
| N-07 | Review or model approval attempted by `automatic_parser` or `llm_assistant` (or `automated_check` approval) | Rejected by `apply_review_decision` (`unauthorized_reviewer_kind`) and `validate_model_validation_record` / `evaluate_rc_model_compatibility` (`model_unvalidated`). |
| N-08 | Missing, unvalidated, failed, stale-version, or subcircuit model | Rejected by `evaluate_rc_model_compatibility` (`model_unvalidated`, `model_failed_validation`, `model_version_mismatch`, `model_validation_out_of_date`, `unsupported_model_format`); `execute_vertical_rc_run` throws before creating output directory. |
| N-09 | Operating conditions outside supported validity envelope (`150 degC`, `60 V`, `5 MHz`) or under `ExecutionPolicy::engineering_approved_only` | Rejected by `evaluate_rc_model_compatibility` (`operating_condition_outside_validity_envelope`, `synthetic_data_prohibited_in_engineering_mode`); `execute_vertical_rc_run` throws before creating output directory. |
| N-10 | Missing Poppler or `ngspice` executable | Missing `pdfinfo`/`pdftotext` yields `!candidate_bundle.has_value()` with diagnostic; missing `ngspice` throws `std::runtime_error` in `execute_vertical_rc_run` with no `results.csv`. |
| D-01 | Repeat extraction and repeat assembly/simulation determinism | Repeated `extract_electrical_table` calls produce identical `IntakeManifest`, observations, IDs, ordering, `EvidenceLocation`s, and diagnostics; repeated assembly and simulation produce identical content IDs (`configuration_content_id`, `netlist_content_id`, `provenance_content_id`) and byte-identical simulation artifacts. |

## 5. Out-of-scope constraints

Phase 4D does not implement GUI/Qt/OpenGL, OCR, arbitrary-layout PDF table detection,
LLM/Ollama integration, persistent databases or general JSON serialization, automatic
engineering approval or model selection, automatic unit conversion, semiconductor or
nonlinear device models, a custom SPICE solver, vendor datasheet downloads/redistribution,
Poppler bundling, or a repository license.

