# Phase 3 plan: Deterministic component-to-simulation vertical slice

Status: **IMPLEMENTED and VERIFIED** on `main` at
`6516a629c75662b83f48704bfd7978bcc299ff78` (initially implemented on
`feat/phase-3-vertical-slice` from `b2f73d5` and verified in hosted Windows CI
[run 41284202716](https://github.com/quocthaile/datasim/actions/runs/41284202716)),
with architecture and validation hardening on `fix/phase3-validation-hardening`. See
[Phase 3 implementation](PHASE_3_IMPLEMENTATION.md) and
[Phase 3 validation](PHASE_3_VALIDATION.md) for actual deliverables and test outcomes.

## Preconditions verified

1. Phase 1/1.5 (`86b924b`, PR #1) and Phase 2 (`b2f73d5`, PR #2) are merged into
   `main`.
2. Clean baseline build and CTest verification on `feat/phase-3-vertical-slice`
   (branched from `b2f73d5`) passed 4/4 test suites (`unit`: 92 checks,
   `component_model`: 270 checks, `backend_errors`, and `ngspice_rc`: 640 samples,
   `max_error_V = 1.42982e-5`, 0 skips).
3. Following Phase 3 merge into `main` at `6516a629c75662b83f48704bfd7978bcc299ff78`,
   hosted Windows CI [run 41284202716](https://github.com/quocthaile/datasim/actions/runs/41284202716)
   and local baseline verification on `fix/phase3-validation-hardening` passed 5/5
   CTest suites (`unit`: 92 checks, `component_model`: 270 checks,
   `vertical_slice`: 1392 checks, `backend_errors`, and `ngspice_rc` with 0 skips) and
   the six-run convergence/repeatability study.
4. All architecture documents and ADRs ([ADR-0001](decisions/ADR-0001-simulation-backend-boundary.md),
   [ADR-0002](decisions/ADR-0002-phase-1-ngspice-executable.md),
   [ADR-0003](decisions/ADR-0003-component-domain-model.md), and
   [ADR-0004](decisions/ADR-0004-phase-3-vertical-slice.md)) have been reviewed.

## Objective

Connect approved `ComponentKnowledge` and explicitly selected
`ModelArtifactDescriptor` instances to the existing ideal-RC `ngspice` simulation
backend through a deterministic, traceable, fail-closed C++20 vertical slice
(`datasim_vertical`).

## Work packages

| Work package | Module path | Responsibilities |
| --- | --- | --- |
| WP1: Circuit assembly | `src/datasim/vertical/assembly/` | `CircuitAssembly`, `CircuitComponentInstance`, `TerminalNodeBinding`, `validate_circuit_assembly`: multi-bundle circuit aggregate preserving component identity, knowledge/model version pins, and 1-based terminal-to-pin-to-node bindings without changing `ComponentDomainBundle`. |
| WP2: Parameter resolution | `src/datasim/vertical/resolution/` | `ResolvedComponentParameter`, `ResolvedRcParameters`, `resolve_rc_parameters`: resolve approved typical `dc_resistance` (`ohm`) and `capacitance` (`F`), preserve `ObservationId`, `EvidenceLocation`, `SourceDocument`, `conversion_rule`, and `OperatingCondition`, and reject missing, conflicting, non-typical, non-SI, or unsupported unit mappings. |
| WP3: Model/backend compatibility & policy gate | `src/datasim/vertical/compatibility/` | `ExecutionPolicy`, `ExecutionClassification`, `OperatingEnvironment`, `ModelCompatibilityReport`, `conservative_peak_resistor_power_W`, `is_supported_rc_envelope_quantity`, `evaluate_rc_model_compatibility`: enforce `ModelFormat::analytical_equation` for the ideal-RC backend, reject `spice_subcircuit` and other external/unsupported formats without assuming artifact availability, verify `ModelValidationRecord` domain validity (`ActorKind::human_engineer` reviewer, timestamp, evidence references, and `validated_within_envelope` on matching version), evaluate every `ModelValidityEnvelope` constraint fail-closed against explicit runtime conditions, and enforce the metadata gate between `test_only_synthetic` and `engineering_approved` execution. |
| WP4: Simulation adapter | `src/datasim/vertical/adapter/` | `RcPulseStimulus`, `RcTransientControls`, `RcSolverControls`, `RcRunInputSpecification`, `AdaptedRcSimulation`, `prepare_vertical_rc_simulation`: construct `datasim::RcConfig` from resolved component parameters while keeping stimulus, timing, and solver controls separate and preserving Phase 1/1.5 solver behavior. |
| WP5: Provenance | `src/datasim/vertical/provenance/` | `ComponentProvenanceRecord`, `VerticalSliceProvenanceManifest`, `VerticalRcRunResult`, `provenance_json`, `execute_vertical_rc_run`: record component/knowledge/model/parameter/observation/evidence IDs, conversion rules, terminal bindings, and non-cryptographic FNV-1a-64 configuration/netlist/provenance content identities; emit `provenance_metadata.json` alongside existing Phase 1.5 run artifacts. |
| WP6: Verification & hardening | `tests/unit/vertical_slice_tests.cpp`, `tests/integration/main.cpp` | Unit and real `ngspice-47` integration tests covering malformed references, missing/incompatible units, model-version mismatch, invalid/non-human `ModelValidationRecord` entries, unsupported model formats (`spice_subcircuit`, etc.), fail-closed validity-envelope evaluation (missing/null/non-finite/conflicting/unknown conditions, inclusive/exclusive boundaries, conservative peak-power screening), synthetic vs non-synthetic metadata policy gates, exact configuration/netlist equivalence with manual `RcConfig`, identical numerical waveform outputs, and Phase 1.5 convergence/repeatability preservation. |

## Explicit non-goals

- No PDF/OCR ingestion or curve digitization.
- No LLM or Ollama integration.
- No database or general persistence architecture.
- No GUI or visualization framework.
- No semiconductor device models or external SPICE subcircuit loader.
- No custom SPICE solver or GPU implementation.
- No real manufacturer datasheet ingestion or physical hardware measurement (`MEASURED`).
