# Phase 3 implementation: Deterministic component-to-simulation vertical slice

Status: **IMPLEMENTED** and **VERIFIED** locally on Windows x64 (MSYS2 UCRT GCC
16.2.0, C++20, Release, ngspice-47). See [Phase 3 validation](PHASE_3_VALIDATION.md)
for exact test counts, numerical equivalence evidence, and Phase 1.5 non-regression
checks.

## Baseline and branch

Work started from `origin/main` at `b2f73d5` (merge commit of PR #2 completing
Phase 2) on feature branch `feat/phase-3-vertical-slice`. Before any source edit, the
baseline checkout built cleanly and passed 4/4 CTest suites (`unit`: 92 checks,
`component_model`: 270 checks, `backend_errors`, and real `ngspice_rc` integration
with 0 skips).

## Module layout and boundaries

Phase 3 adds a standalone C++20 static library target `datasim_vertical` in
`namespace datasim::vertical`, linking `datasim_component` and `datasim` while
leaving both underlying libraries unchanged in their single-component and solver
responsibilities ([ADR-0004](decisions/ADR-0004-phase-3-vertical-slice.md)):

| Path | Public types and functions | Responsibility |
| --- | --- | --- |
| `src/datasim/vertical/validation/vertical_validation.hpp` | `VerticalValidationCode`, `VerticalValidationIssue`, `VerticalValidationReport`, `ensure_vertical_valid` | 42 deterministic validation rule codes and fail-fast exception gating across assembly, resolution, compatibility, and adapter stages. |
| `src/datasim/vertical/assembly/circuit_assembly.hpp`, `.cpp` | `CircuitAssemblyId`, `ComponentInstanceId`, `CircuitTopologyKind`, `CircuitElementRole`, `TerminalNodeBinding`, `CircuitComponentInstance`, `CircuitAssembly`, `find_bundle_for_component`, `find_model_in_bundle`, `find_instance_by_role`, `validate_circuit_assembly` | Application-level multi-bundle circuit aggregate preserving per-component identity, knowledge/model version pins, and 1-based terminal-to-pin-to-node bindings without modifying `ComponentDomainBundle`. |
| `src/datasim/vertical/resolution/parameter_resolution.hpp`, `.cpp` | `ResolvedComponentParameter`, `ResolvedRcParameters`, `ParameterResolutionResult`, `resolve_rc_parameters` | Resolves approved typical `dc_resistance` (`ohm`) and `capacitance` (`F`), preserves `ObservationId`, `EvidenceLocation`, `SourceDocument`, original notation, `conversion_rule`, and `OperatingCondition`, and rejects missing, conflicting, non-typical, non-SI, or unsupported unit mappings. |
| `src/datasim/vertical/compatibility/model_compatibility.hpp`, `.cpp` | `ExecutionPolicy`, `ExecutionClassification`, `OperatingEnvironment`, `ModelCompatibilityReport`, `is_model_format_supported_by_rc_backend`, `is_analysis_supported_by_rc_backend`, `evaluate_rc_model_compatibility` | Enforces `ModelFormat::analytical_equation` and `AnalysisType::transient` for the ideal-RC backend, rejects `spice_subcircuit` and other external formats without assuming external artifact availability, checks `ModelValidationRecord` status (`validated_within_envelope` on matching version) and `ModelValidityEnvelope` ranges, and distinguishes `test_only_synthetic` from `engineering_approved` execution. |
| `src/datasim/vertical/adapter/rc_simulation_adapter.hpp`, `.cpp` | `RcPulseStimulus`, `RcTransientControls`, `RcSolverControls`, `RcRunInputSpecification`, `AdaptedRcSimulation`, `RcAdapterPreparationResult`, `build_rc_config_from_resolved`, `prepare_vertical_rc_simulation`, `adapt_vertical_rc_simulation` | Separates voltage stimulus, transient timing, and solver/temperature controls from component knowledge; constructs `datasim::RcConfig` deterministically without introducing a new solver. |
| `src/datasim/vertical/provenance/run_provenance.hpp`, `.cpp` | `ComponentProvenanceRecord`, `VerticalSliceProvenanceManifest`, `VerticalRcRunResult`, `build_provenance_manifest`, `provenance_json`, `execute_vertical_rc_run` | Records full component, knowledge, model, parameter, observation, evidence, conversion rule, terminal binding, and configuration/netlist/provenance hash lineage; emits `provenance_metadata.json` alongside existing Phase 1.5 run artifacts. |

## Build and test commands

From the repository root in PowerShell on Windows x64 (MSYS2 UCRT GCC 16.2.0):

```powershell
cmake -S . -B build/phase3-wp6 -G Ninja -DCMAKE_BUILD_TYPE=Release -DDATASIM_NGSPICE=D:/projects/datasim/build/tools/msys2-ngspice/ucrt64/bin/ngspice_con.exe
cmake --build build/phase3-wp6
ctest --test-dir build/phase3-wp6 --output-on-failure -V
```

## Limitations

1. **Ideal-RC topology only**: `CircuitAssembly` and `datasim_vertical` currently
   support only `CircuitTopologyKind::series_rc_lowpass` (`vin`, `vout`, `0`) with
   ideal two-terminal resistor and capacitor models (`ModelFormat::analytical_equation`)
   in transient analysis (`AnalysisType::transient`).
2. **No external subcircuit or model-card loader**: `ModelFormat::spice_subcircuit`,
   `spice_model_card`, `lookup_table`, and `vendor_specific_artifact` are
   deterministically rejected (`unsupported_model_format` and
   `external_artifact_unavailable_in_rc_backend`).
3. **Fixed Phase 1/1.5 solver options**: `RcSolverControls` enforces the verified
   Phase 1/1.5 settings (`method = "trap"`, `reltol = 1e-6`, `abstol = 1e-12`,
   `vntol = 1e-9`, `temp = 27 degC`, `tnom = 27 degC`) and rejects unsupported solver
   overrides rather than silently ignoring them.
4. **In-memory assembly**: Persistence, PDF/OCR ingestion, automatic unit conversion
   algebra, LLM integration, semiconductor models, and GUI visualization remain
   `PROPOSED`.

Related: [Phase 3 plan](PHASE_3_PLAN.md),
[Phase 3 validation](PHASE_3_VALIDATION.md),
[ADR-0004](decisions/ADR-0004-phase-3-vertical-slice.md).

