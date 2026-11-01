# ADR-0003: Phase 2 in-memory C++20 component domain model and knowledge contracts

Date: 2026-10-31

Status: SELECTED and IMPLEMENTED for Phase 2 on `feat/phase-2-component-model`;
verification evidence is recorded in
[Phase 2 validation](../PHASE_2_VALIDATION.md) and
[Phase 2 implementation](../PHASE_2_IMPLEMENTATION.md).

## Context and requirement

With Phase 1 and Phase 1.5 merged into `main` (`86b924b4b49d69c70c1688a8748cb1c2893adc34`),
Datasim has a verified ideal-RC/ngspice execution slice using manually configured
numbers, while the conceptual data contracts in
[COMPONENT_DATA_MODEL.md](../COMPONENT_DATA_MODEL.md) and
[ARCHITECTURE.md](../ARCHITECTURE.md) remained prose specifications. Before any
datasheet ingestion, storage schema, assisted extraction, or datasheet-backed
simulation coupling can be built safely, Datasim needs executable C++20 domain
types and deterministic validation rules that enforce provenance, unit/qualifier
preservation, accountable engineering review, visible conflict handling, and the
separation between component facts and mathematical models.

Phase 2 requires this domain layer to remain strictly independent of ngspice,
process execution, filesystem persistence, PDF/OCR parsing, general-purpose JSON
serialization, automatic unit conversion, LLM runtimes, and GUI frameworks.

## Decision

1. **Isolated C++20 library module (`datasim_component`)**:
   Implement the domain model under `src/datasim/component/` in
   `namespace datasim::component`, compiled as a standalone CMake static library
   target (`datasim_component`) using only the C++20 standard library. Do not link
   `datasim_component` against `src/datasim/simulation/` or modify existing Phase 1
   RC simulation modules.
2. **Value types and strong identifiers**:
   Represent domain entities as explicit C++20 value structs (`ComponentIdentity`,
   `SourceDocument`, `EvidenceLocation`, `OperatingCondition`, `ReviewDecision`,
   `ParameterObservation`, `NormalizedParameter`, `ParameterConflict`,
   `ComponentKnowledge`, `ModelArtifactDescriptor`, `ModelValidityEnvelope`,
   `ModelValidationRecord`) keyed by tag-parameterized `StrongId<Tag>` types with
   defaulted `<=>` comparisons. Use `std::optional<double>` so absent datasheet
   values remain absent rather than defaulting to `0.0`.
3. **Single-qualifier observations and explicit conversions**:
   Each `ParameterObservation` carries one `ValueQualifier` (`minimum`, `typical`,
   `maximum`, or `unspecified`) and one explicit `ParameterCategory`, preventing
   silent merging of min/typ/max bounds or inference of stress limits as operating
   conditions. `NormalizedParameter` requires an explicit `conversion_rule` string,
   retains `source_observation_id`, and preserves original value text and unit
   symbols verbatim.
4. **Deterministic human-gated review state machine**:
   Enforce transitions `candidate -> needs_review -> approved | rejected` and
   `approved -> superseded` via `apply_review_decision` and full `review_history`
   audit verification. Require `ActorKind::human_engineer` for `approved`,
   `rejected`, and `superseded` transitions, and reject self-approval where
   `reviewer.id` matches `extraction_tool` or uses an automated/LLM actor kind.
5. **Four-way separation of trust**:
   Keep (a) `ParameterObservation::review_state`,
   (b) `ComponentKnowledge::review_decision`,
   (c) `ModelArtifactDescriptor` structural registration validity, and
   (d) `ModelValidationRecord::status` (`effective_model_validation_status`)
   separate. Block behavioral `ModelParameterMapping` bindings from referencing
   `absolute_maximum_rating` parameters.
6. **Structured deterministic validation (`ValidationReport`)**:
   Return ordered `ValidationIssue` entries carrying stable `ValidationRuleCode`
   enumerators and entity paths for both per-entity validation and cross-entity
   `ComponentDomainBundle` referential integrity checks, paired with `ensure_valid`
   for fail-fast callers. Enforce physical dimensional compatibility for model parameter
   mappings (requiring explicit conversion contracts for differing units within the same
   dimension while prohibiting automatic conversion or cross-dimension mapping), and
   enforce exact component part number applicability for source documents (preserving
   suffixes, prohibiting prefix-only matching, and supporting multi-part family documents).

## Alternatives considered

- **Coupling domain types directly to `RcConfig` or ngspice netlists**: Rejected.
  Phase 1 `RcConfig` represents a narrow ideal-circuit test harness, whereas the
  component domain model must represent multi-condition datasheet observations,
  conflicts, and model metadata without leaking simulator dialect assumptions
  ([ADR-0001](ADR-0001-simulation-backend-boundary.md)).
- **Polymorphic class hierarchies or generic property-bag maps**: Rejected. Deep
  inheritance and untyped `map<string, any>` containers obscure required fields,
  weaken compile-time invariants, and invite premature framework complexity.
- **Adding a JSON/SQLite/unit-conversion third-party library now**: Rejected.
  Persistence, interchange schemas, and dimensional algebra engines are out of
  Phase 2 scope and would lock in premature storage/serialization dependencies
  before the core domain invariants are verified in memory.
- **Exception-only validation without rule codes**: Rejected. While `ensure_valid`
  provides exception-based gating, structured `ValidationRuleCode` reports allow
  unit tests and future review UIs to inspect every violated rule deterministically
  without fragile substring matching.

## Compatibility and consequences

- Existing `datasim` simulation library, `rc_demo`, `rc_study`, CSV/JSON schemas,
  `0.05 V` tolerance, and all Phase 1/1.5 unit, fault, and ngspice integration
  tests remain untouched.
- `CMakeLists.txt` source-hash computation (`DATASIM_SOURCE_SHA256`) naturally
  incorporates new `src/*.cpp`, `src/*.hpp`, and `tests/*.cpp` files as designed in
  Phase 1.5 to identify the exact repository source tree.
- Persistence, datasheet ingestion, unit algebra, and knowledge-to-simulation model
  instantiation remain explicit future work requiring separate authorization and
  ADRs.

Related: [Phase 2 plan](../PHASE_2_PLAN.md),
[Phase 2 implementation](../PHASE_2_IMPLEMENTATION.md),
[Phase 2 validation](../PHASE_2_VALIDATION.md),
[component data model](../COMPONENT_DATA_MODEL.md),
[architecture](../ARCHITECTURE.md),
[ADR-0001](ADR-0001-simulation-backend-boundary.md),
[ADR-0002](ADR-0002-phase-1-ngspice-executable.md).
