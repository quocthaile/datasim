# Phase 3 implementation: Deterministic component-to-simulation vertical slice

Status: **IMPLEMENTED** and **VERIFIED** on Windows x64 (MSYS2 UCRT GCC 16.2.0,
C++20, Release, ngspice-47), both on `main`
(`6516a629c75662b83f48704bfd7978bcc299ff78`, hosted Windows CI
[run 41284202716](https://github.com/quocthaile/datasim/actions/runs/41284202716))
and on `fix/phase3-validation-hardening`. See
[Phase 3 validation](PHASE_3_VALIDATION.md) for exact test counts, numerical
equivalence evidence, and Phase 1.5 non-regression checks.

## Baseline and branch history

1. **Initial Phase 3 implementation (`feat/phase-3-vertical-slice` -> `main`)**:
   Started from `origin/main` at `b2f73d5` (merge commit of PR #2 completing Phase 2)
   and merged into `main` at `6516a629c75662b83f48704bfd7978bcc299ff78`, where hosted
   Windows CI [run 41284202716](https://github.com/quocthaile/datasim/actions/runs/41284202716)
   passed all 5 CTest suites (`unit`: 92 checks, `component_model`: 270 checks,
   `vertical_slice`: 1392 checks, `backend_errors`, and real `ngspice_rc` integration
   with 0 skips) and the six-run study.
2. **Phase 3 architecture and validation hardening (`fix/phase3-validation-hardening`)**:
   Branched from `origin/main` at `6516a629c75662b83f48704bfd7978bcc299ff78` to harden
   fail-closed `ModelValidityEnvelope` validation, `ModelValidationRecord` and
   synthetic metadata trust-boundary checks, and provenance content-identity
   documentation without altering `datasim` or `datasim_component`.

## Module layout and boundaries

Phase 3 adds a standalone C++20 static library target `datasim_vertical` in
`namespace datasim::vertical`, linking `datasim_component` and `datasim` while
leaving both underlying libraries unchanged in their single-component and solver
responsibilities ([ADR-0004](decisions/ADR-0004-phase-3-vertical-slice.md)):

| Path | Public types and functions | Responsibility |
| --- | --- | --- |
| `src/datasim/vertical/validation/vertical_validation.hpp` | `VerticalValidationCode`, `VerticalValidationIssue`, `VerticalValidationReport`, `ensure_vertical_valid` | 48 deterministic validation rule codes and fail-fast exception gating across assembly, resolution, compatibility, and adapter stages. |
| `src/datasim/vertical/assembly/circuit_assembly.hpp`, `.cpp` | `CircuitAssemblyId`, `ComponentInstanceId`, `CircuitTopologyKind`, `CircuitElementRole`, `TerminalNodeBinding`, `CircuitComponentInstance`, `CircuitAssembly`, `find_bundle_for_component`, `find_model_in_bundle`, `find_instance_by_role`, `validate_circuit_assembly` | Application-level multi-bundle circuit aggregate preserving per-component identity, knowledge/model version pins, and 1-based terminal-to-pin-to-node bindings without modifying `ComponentDomainBundle`. |
| `src/datasim/vertical/resolution/parameter_resolution.hpp`, `.cpp` | `ResolvedComponentParameter`, `ResolvedRcParameters`, `ParameterResolutionResult`, `resolve_rc_parameters` | Resolves approved typical `dc_resistance` (`ohm`) and `capacitance` (`F`), preserves `ObservationId`, `EvidenceLocation`, `SourceDocument`, original notation, `conversion_rule`, and `OperatingCondition`, and rejects missing, conflicting, non-typical, non-SI, or unsupported unit mappings. |
| `src/datasim/vertical/compatibility/model_compatibility.hpp`, `.cpp` | `ExecutionPolicy`, `ExecutionClassification`, `OperatingEnvironment`, `ModelCompatibilityReport`, `is_model_format_supported_by_rc_backend`, `is_analysis_supported_by_rc_backend`, `conservative_peak_resistor_power_W`, `is_supported_rc_envelope_quantity`, `evaluate_rc_model_compatibility` | Enforces `ModelFormat::analytical_equation` and `AnalysisType::transient` for the ideal-RC backend, rejects `spice_subcircuit` and other external formats without assuming external artifact availability, checks `ModelValidationRecord` structural/reviewer validity and `validated_within_envelope` status on the exact model version, evaluates every `ModelValidityEnvelope` constraint fail-closed, and enforces the `ExecutionPolicy` metadata gate. |
| `src/datasim/vertical/adapter/rc_simulation_adapter.hpp`, `.cpp` | `RcPulseStimulus`, `RcTransientControls`, `RcSolverControls`, `RcRunInputSpecification`, `AdaptedRcSimulation`, `RcAdapterPreparationResult`, `build_rc_config_from_resolved`, `prepare_vertical_rc_simulation`, `adapt_vertical_rc_simulation` | Separates voltage stimulus, transient timing, and solver/temperature controls from component knowledge; constructs `datasim::RcConfig` deterministically without introducing a new solver. |
| `src/datasim/vertical/provenance/run_provenance.hpp`, `.cpp` | `ComponentProvenanceRecord`, `VerticalSliceProvenanceManifest`, `VerticalRcRunResult`, `build_provenance_manifest`, `provenance_json`, `execute_vertical_rc_run` | Records full component, knowledge, model, parameter, observation, evidence, conversion rule, terminal binding, and non-cryptographic FNV-1a-64 configuration/netlist/provenance content identities; emits `provenance_metadata.json` alongside existing Phase 1.5 run artifacts. |

## Fail-closed `ModelValidityEnvelope` constraint mapping

`evaluate_rc_model_compatibility()` evaluates every `NumericRangeConstraint` in
`ModelValidityEnvelope::operating_ranges` fail-closed. No constraint may silently pass
when its runtime condition is absent, non-numeric, non-finite, unit-mismatched,
ambiguous, or unrecognized:

| Envelope `quantity_name` | Accepted `OperatingCondition` aliases | Required `unit_symbol` | Applicable `CircuitElementRole` | Runtime value source and evaluation rule |
| --- | --- | --- | --- | --- |
| `ambient_temperature` | `ambient_temperature`, `T_A`, `T_ambient` | `degC` | `series_resistor`, `shunt_capacitor` | Primary runtime source: `OperatingEnvironment::ambient_temperature_degC` (populated from `RcRunInputSpecification::solver.ambient_temperature_degC`). Must be present (`has_value()`) and finite. Any matching `OperatingCondition` in `resolved_param.operating_conditions` or `environment.additional_conditions` must also use `degC`, provide a finite `numeric_value`, and lie within the constraint range; an `additional_conditions` entry that disagrees with `ambient_temperature_degC` is rejected. |
| `terminal_dc_voltage` | `terminal_dc_voltage`, `V_DC`, `V_terminal`, `voltage_V` | `V` | `series_resistor`, `shunt_capacitor` | Primary runtime sources: `OperatingEnvironment::stimulus_voltage_V` and `OperatingEnvironment::initial_voltage_V` (populated from `RcRunInputSpecification::stimulus`), plus the step differential `stimulus_voltage_V - initial_voltage_V` for `series_resistor`. Both voltage fields must be present and finite (`stimulus_voltage_V > 0`). Any matching `additional_conditions` entry must use `V`, provide a finite `numeric_value`, and agree with `stimulus_voltage_V`. |
| `continuous_power` | `continuous_power`, `P_load`, `P_continuous`, `power_W` | `W` | `series_resistor` only (rejected on `shunt_capacitor`) | Evaluated via the conservative electrical peak-power screening rule `conservative_peak_resistor_power_W(stimulus_voltage_V, initial_voltage_V, resistance_ohm) = max(V_stimulus^2, V_initial^2, (V_stimulus - V_initial)^2) / R`. Because steady-state and time-average ohmic dissipation in a passive series-RC step response cannot exceed the instantaneous peak ohmic power, comparing $P_{\text{peak}}$ against the `continuous_power` ceiling provides a conservative electrical screening bound without claiming a transient thermal-impedance or physical derating model. |
| `signal_frequency` | `signal_frequency`, `f_signal`, `frequency_Hz` | `Hz` | `series_resistor`, `shunt_capacitor` | Must be explicitly supplied in `OperatingEnvironment::additional_conditions` (`RcRunInputSpecification::additional_operating_conditions`) with unit `Hz` and a present, finite `numeric_value`. Never silently inferred from transient pulse timing (`period_s`, `width_s`, or `rise_s`), and never conflated with datasheet LCR characterization conditions such as `f_test`. |
| Any other `quantity_name` | — | — | None | Deterministically rejected with `VerticalValidationCode::invalid_operating_condition`. |

Duplicate conditions for the same canonical quantity within `additional_conditions` or
within `resolved_param.operating_conditions` are accepted only when their units and
finite numeric values agree; conflicting or non-numeric duplicates are rejected.

## `ExecutionPolicy` and synthetic fixture trust boundary

`ExecutionPolicy` (`engineering_approved_only` by default vs `test_only_synthetic`)
and `ExecutionClassification` (`engineering_approved` vs `test_only_synthetic`)
inspect all in-memory provenance layers of each component:

- `ComponentIdentity::is_synthetic` (on both `bundle.component` and
  `bundle.knowledge->component`)
- `SourceDocument::is_synthetic`
- `ParameterObservation::extraction_method == ExtractionMethod::synthetic_fixture`
- `ModelArtifactDescriptor::is_synthetic`
- `ComponentKnowledge` approval state (`ReviewState::approved` by
  `ActorKind::human_engineer`)
- `ModelValidationRecord` domain validity (`validate_model_validation_record`,
  requiring `ActorKind::human_engineer`, timestamp, resolved `evidence_refs`,
  `verified_analyses`, and `ModelValidationStatus::validated_within_envelope` on the
  exact selected `model_version`)

**Trust boundary limitation**: This gate validates in-memory metadata structures and
review records only. It does **not** authenticate real human reviewer identity,
manufacturer identity, datasheet authenticity, external data-source integrity, or
cryptographic/organizational authorization. In unit tests,
`make_non_synthetic_policy_gate_rc_assembly()` in
`tests/unit/vertical_slice_fixtures.cpp` is a metadata-classified non-synthetic test
fixture used solely to exercise the `ExecutionPolicy::engineering_approved_only`
branch; it is derived from synthetic passive fixtures and is not real manufacturer or
engineering-qualified component data.

## Provenance and content-identity audit

`VerticalSliceProvenanceManifest` (`src/datasim/vertical/provenance/run_provenance.hpp`,
`.cpp`) computes three 64-bit FNV-1a (`datasim::content_id`) content fingerprints in
`build_provenance_manifest()` **before** backend subprocess execution:

1. `configuration_content_id`:
   - Computed over `datasim::config_json(adapted.rc_config)` (`std::locale::classic()`,
     `std::setprecision(17)`).
   - Participating fields: `parameter_origin` (`"manual_ideal_components"` in
     `RcConfig`), `resistance_ohm`, `capacitance_F`, `voltage_V`, `initial_voltage_V`,
     `source_initial_V`, `delay_s`, `rise_s`, `fall_s`, `width_s`, `period_s`,
     `requested_step_s`, `maximum_step_s`, `duration_s`, `method`, `reltol`,
     `abstol_A`, `vntol_V`, `temperature_degC`, `tnom_degC`.
2. `netlist_content_id`:
   - Computed over the exact LF-normalized SPICE netlist string returned by
     `datasim::rc_netlist(adapted.rc_config)`.
3. `provenance_content_id`:
   - Computed **before** backend execution over `canonical_provenance_payload(manifest)`,
     a compact single-line JSON object (`std::locale::classic()`,
     `std::setprecision(17)`) with fixed key order:
     `{"circuit_id":...,"execution_classification":...,"is_synthetic_test_run":...,"resistor":{...},"capacitor":{...},"configuration_content_id":...,"netlist_content_id":...}`.
   - Participating fields in each `ComponentProvenanceRecord` (`resistor` and
     `capacitor`): `instance_id`, `role`, `component_id`, `manufacturer`,
     `part_number`, `part_number_suffix`, `is_synthetic`, `knowledge_id`,
     `knowledge_version`, `knowledge_review_decision_id`, `knowledge_reviewer_id`,
     `model_id`, `model_version`, `model_format`, `model_validation_status`,
     `model_validation_record_ids`, `model_parameter_name`, `normalized_parameter_id`,
     `canonical_parameter_name`, `qualifier`, `category`, `resolved_si_value`,
     `normalized_si_unit`, `source_observation_id`, `preserved_original_value_text`,
     `preserved_original_unit_symbol`, `conversion_rule`, `operating_conditions`,
     `evidence_location_ids`, `source_document_ids`, `terminal_bindings`.
   - Excluded from `provenance_content_id` (recorded separately in
     `provenance_metadata.json`): `schema_version`, `circuit_name`, `parameter_origin`,
     `backend_name`, `backend_version`, `backend_executable`, `execution_status`,
     `validation_status`, `error`, `application_version`, `git_revision_at_configure`,
     `source_sha256`, and post-run output files (`results.csv`, `errors.csv`,
     `samples.dat`, `ngspice.log`).
4. **FNV-1a-64 limitations and run reproducibility**:
   - FNV-1a-64 is a non-cryptographic content fingerprint for detecting accidental
     configuration, netlist, or lineage drift across runs. It is **not** cryptographic
     integrity protection, a digital signature, tamper-proof provenance, or
     authentication of source evidence.
   - Combined with `run_metadata.json`, `validation_report.json`, and `circuit.cir`,
     `provenance_metadata.json` records all identifiers, versions, conversion rules,
     terminal bindings, and solver controls needed to audit and reproduce the ideal-RC
     simulation run from the in-memory component bundles. Full archival serialization
     of complete `ComponentDomainBundle` graphs and cryptographic artifact signing
     belong to future persistence/security ADRs rather than Phase 3.

## Build and test commands

From the repository root in PowerShell on Windows x64 (MSYS2 UCRT GCC 16.2.0):

```powershell
$env:PATH = "C:\msys64\ucrt64\bin;$env:PATH"
cmake -S . -B build/phase3-hardening -G Ninja -DCMAKE_BUILD_TYPE=Release -DDATASIM_NGSPICE=D:/projects/datasim/build/tools/msys2-ngspice/ucrt64/bin/ngspice_con.exe
cmake --build build/phase3-hardening
ctest --test-dir build/phase3-hardening --output-on-failure -V
python -m unittest discover -s tests/ci -v
./build/phase3-hardening/rc_study.exe D:/projects/datasim/build/tools/msys2-ngspice/ucrt64/bin/ngspice_con.exe build/phase3-hardening-study
```

## Limitations

1. **Ideal-RC topology only**: `CircuitAssembly` and `datasim_vertical` support only
   `CircuitTopologyKind::series_rc_lowpass` (`vin`, `vout`, `0`) with ideal
   two-terminal resistor and capacitor models (`ModelFormat::analytical_equation`) in
   transient analysis (`AnalysisType::transient`).
2. **No external subcircuit or model-card loader**: `ModelFormat::spice_subcircuit`,
   `spice_model_card`, `lookup_table`, and `vendor_specific_artifact` are
   deterministically rejected (`unsupported_model_format` and
   `external_artifact_unavailable_in_rc_backend`).
3. **Fixed Phase 1/1.5 solver options**: `RcSolverControls` enforces the verified
   Phase 1/1.5 settings (`method = "trap"`, `reltol = 1e-6`, `abstol = 1e-12`,
   `vntol = 1e-9`, `temp = 27 degC`, `tnom = 27 degC`) and rejects unsupported solver
   overrides rather than silently ignoring them.
4. **In-memory assembly and unauthenticated metadata**: Persistence, PDF/OCR ingestion,
   real manufacturer datasheet ingestion, physical measurement validation (`MEASURED`),
   automatic unit conversion algebra, LLM integration, nonlinear semiconductor models,
   and GUI visualization remain `PROPOSED`.

Related: [Phase 3 plan](PHASE_3_PLAN.md),
[Phase 3 validation](PHASE_3_VALIDATION.md),
[ADR-0004](decisions/ADR-0004-phase-3-vertical-slice.md).
