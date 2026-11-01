# ADR-0004: Phase 3 deterministic component-to-simulation vertical slice

Date: 2026-11-01

Status: SELECTED, IMPLEMENTED, and VERIFIED on `main`
(`6516a629c75662b83f48704bfd7978bcc299ff78`, hosted Windows CI
[run 41284202716](https://github.com/quocthaile/datasim/actions/runs/41284202716)),
with validation hardening on `fix/phase3-validation-hardening`; verification evidence
is recorded in [Phase 3 validation](../PHASE_3_VALIDATION.md) and
[Phase 3 implementation](../PHASE_3_IMPLEMENTATION.md).

## Context and requirement

With Phase 1/1.5 (`86b924b4b49d69c70c1688a8748cb1c2893adc34`, PR #1) and Phase 2
(`b2f73d5`, PR #2) merged into `main`, Datasim has two isolated, verified C++20
modules:

1. `datasim` (`src/datasim/simulation/`): manual ideal-RC configuration (`RcConfig`),
   shell-free `ngspice-47` execution, common-grid validation (`0.05 V` tolerance),
   and JSON/CSV run reporting ([ADR-0002](ADR-0002-phase-1-ngspice-executable.md)).
2. `datasim_component` (`src/datasim/component/`): single-component in-memory domain
   contracts (`ComponentIdentity`, `SourceDocument`, `EvidenceLocation`,
   `ParameterObservation`, `ReviewDecision`, `NormalizedParameter`,
   `ComponentKnowledge`, `ModelArtifactDescriptor`, `ModelValidationRecord`, and
   `ComponentDomainBundle`) ([ADR-0003](ADR-0003-component-domain-model.md)).

Phase 3 authorizes the deterministic vertical slice (Roadmap Stage 4 scoped to the
existing ideal-RC transient backend): connecting approved `ComponentKnowledge` and
explicitly selected `ModelArtifactDescriptor` instances to `RcConfig` and the
existing `ngspice` execution pipeline while preserving full provenance from source
evidence to simulation output.

Key architectural constraints:

- Do not alter `ComponentDomainBundle`'s single-component responsibility or couple
  `datasim_component` to simulation headers or `<filesystem>`.
- Do not modify Phase 1/1.5 solver behavior, `RcConfig` semantics, CSV schemas, or
  the 1% (`0.05 V`) step-amplitude tolerance.
- Do not assume that valid `ModelArtifactDescriptor` metadata for an external format
  (such as `ModelFormat::spice_subcircuit`) means the external artifact is present or
  executable by the ideal-RC backend.
- Distinguish test-only synthetic execution (`is_synthetic == true` or
  `ExtractionMethod::synthetic_fixture`) from non-synthetic execution via an explicit
  metadata policy gate without claiming external identity or datasheet authentication.

## Decision

1. **Dedicated application-layer vertical-slice module (`datasim_vertical`)**:
   Implement Phase 3 under `src/datasim/vertical/` in `namespace datasim::vertical`
   as a separate CMake static library target `datasim_vertical` that links
   `datasim_component` and `datasim`. Both underlying libraries remain decoupled from
   each other and unchanged in their existing contracts.
2. **Application-level circuit assembly (`CircuitAssembly`)**:
   Represent multi-component circuits via `CircuitAssembly`, which holds a collection
   of single-component `ComponentDomainBundle` records and placed
   `CircuitComponentInstance` entries (`R1`, `C1`). Each instance pins an exact
   `ComponentId`, `ComponentKnowledgeId`, `expected_knowledge_version`,
   `ModelArtifactId`, `selected_model_version`, `CircuitElementRole`, and explicit
   1-based `TerminalNodeBinding` records mapping `(position, terminal_name,
   physical_pin_id)` to circuit nodes (`vin`, `vout`, `0`).
3. **Strict SI parameter resolution (`resolve_rc_parameters`)**:
   Resolve only `ReviewState::approved` component knowledge free of unresolved
   conflicts. Require `ValueQualifier::typical` and
   `ParameterCategory::electrical_characteristic` parameters normalized to exact SI
   units (`"ohm"` for resistance, `"F"` for capacitance) with positive finite values
   and audited `conversion_rule` text. Reject missing parameters, conflicting typical
   parameters, non-SI units, cross-dimension mappings, or unverified runtime scaling
   expressions in `ModelParameterMapping`. Preserve `ObservationId`,
   `EvidenceLocation`, `SourceDocument`, original value/unit notation, and
   `OperatingCondition` records in `ResolvedComponentParameter`.
4. **Fail-closed model/backend compatibility and metadata policy gate (`evaluate_rc_model_compatibility`)**:
   For the existing ideal-RC backend:
   - Accept only `ModelFormat::analytical_equation` models with 2 terminals and a
     single constitutive parameter binding (`"R"` for series resistor, `"C"` for
     shunt capacitor).
   - Reject `ModelFormat::spice_subcircuit`, `spice_model_card`, `lookup_table`, and
     `vendor_specific_artifact` without assuming external artifact availability.
   - Require `AnalysisType::transient` support in both `ModelArtifactDescriptor` and
     matching `ModelValidationRecord::verified_analyses`.
   - Validate referenced `ModelValidationRecord` entries for domain validity
     (`ActorKind::human_engineer` reviewer, timestamp, resolved `evidence_refs`,
     supported `verified_analyses`) and require
     `effective_model_validation_status == ModelValidationStatus::validated_within_envelope`
     for the exact selected model version, rejecting `unvalidated`, `failed_validation`,
     dangling/invalid validation records, and version-mismatched `out_of_date` records.
   - Evaluate every `NumericRangeConstraint` in `ModelValidityEnvelope` fail-closed:
     - `ambient_temperature` (`degC`) from `OperatingEnvironment::ambient_temperature_degC`.
     - `terminal_dc_voltage` (`V`) from `OperatingEnvironment::stimulus_voltage_V` and
       `initial_voltage_V` (plus resistor step swing `stimulus_voltage_V - initial_voltage_V`).
     - `continuous_power` (`W`, supported on `series_resistor` only) via the
       conservative electrical peak-power screening bound
       `conservative_peak_resistor_power_W = max(V_stimulus^2, V_initial^2, (V_stimulus - V_initial)^2) / R`,
       documented explicitly as a conservative peak-power screen rather than a thermal
       or physical derating model.
     - `signal_frequency` (`Hz`) required explicitly in
       `OperatingEnvironment::additional_conditions` whenever declared in the model's
       validity envelope (never inferred from transient pulse timing).
     - Reject missing runtime conditions, `std::nullopt` or non-finite numeric values,
       unit mismatches, conflicting duplicate conditions, and unsupported envelope
       constraint quantities.
   - Enforce an explicit `ExecutionPolicy` (`engineering_approved_only` by default vs
     `test_only_synthetic`) across component, `SourceDocument`, observation
     `ExtractionMethod`, knowledge, and model synthetic markers, rejecting synthetic or
     mixed fixtures in engineering mode and labeling synthetic runs as
     `ExecutionClassification::test_only_synthetic`. Document explicitly that this
     policy gate validates in-memory metadata and review records only, not external
     reviewer identity, manufacturer identity, or cryptographic signatures.
5. **Separation of component parameters from stimulus/timing/solver inputs**:
   Keep pulse stimulus (`RcPulseStimulus`), transient timing (`RcTransientControls`),
   and solver/temperature controls (`RcSolverControls`) in `RcRunInputSpecification`,
   separate from component knowledge. Construct `datasim::RcConfig` deterministically
   from `ResolvedRcParameters` + `RcRunInputSpecification` and delegate execution to
   the existing `execute_rc_run` pipeline without creating a new solver.
6. **Reproducible provenance manifest (`VerticalSliceProvenanceManifest`)**:
   Emit `provenance_metadata.json` alongside existing Phase 1.5 run artifacts,
   recording circuit identity, `ExecutionClassification`, per-component IDs,
   knowledge IDs/versions/reviewers, model IDs/versions/formats/validation IDs,
   `NormalizedParameterId` and `ObservationId` links, `EvidenceId` and
   `SourceDocumentId` references, preserved original values/units, `conversion_rule`,
   operating conditions, terminal bindings, and pre-execution FNV-1a-64 content
   fingerprints (`configuration_content_id`, `netlist_content_id`,
   `provenance_content_id`). FNV-1a-64 is documented as a non-cryptographic content
   fingerprint for accidental-drift detection, not cryptographic integrity protection.

## Alternatives considered

- **Adding multi-component circuit fields to `ComponentDomainBundle`**: Rejected.
  Violates `ComponentDomainBundle`'s single-component boundary established in Phase 2
  ([ADR-0003](ADR-0003-component-domain-model.md)).
- **Embedding component pointers inside `datasim::RcConfig`**: Rejected. Would couple
  Phase 1/1.5 simulation code to `datasim_component` and alter existing `RcConfig`
  unit and study contracts.
- **Auto-converting non-SI units inside the simulation adapter**: Rejected. General
  unit conversion is out of scope, and unit normalization belongs in reviewed
  `NormalizedParameter` records with explicit `conversion_rule` provenance.
- **Treating `ModelFormat::spice_subcircuit` metadata as an ideal passive fallback**:
  Rejected. Silently approximating a subcircuit as an ideal element violates
  [ADR-0001](ADR-0001-simulation-backend-boundary.md) and [ARCHITECTURE.md](../ARCHITECTURE.md).
- **Silently skipping `ModelValidityEnvelope` constraints when runtime conditions are omitted**:
  Rejected. Allowing `signal_frequency` or unrecognized constraints to pass without a
  resolved runtime condition violates fail-closed compatibility validation.

## Consequences

- `datasim` and `datasim_component` remain unmodified in their existing APIs and test
  suites.
- Equivalent knowledge-driven and manual `RcConfig` inputs produce identical
  netlists, identical `results.csv` waveforms, and identical common-grid validation
  metrics.
- Every rejection reason is surfaced via deterministic `VerticalValidationCode`
  diagnostics before any workspace directory or backend process is created.

Related: [ADR-0001](ADR-0001-simulation-backend-boundary.md),
[ADR-0002](ADR-0002-phase-1-ngspice-executable.md),
[ADR-0003](ADR-0003-component-domain-model.md),
[Phase 3 plan](../PHASE_3_PLAN.md).
