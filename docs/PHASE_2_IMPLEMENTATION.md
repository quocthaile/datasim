# Phase 2 implementation: Component domain model and knowledge contracts

Status: **IMPLEMENTED** and **VERIFIED** locally on Windows x64 (MSYS2 UCRT GCC
16.2.0, C++20, Release) for in-memory domain contracts and synthetic passive
fixtures. See [Phase 2 validation](PHASE_2_VALIDATION.md) for exact test counts,
Phase 1/1.5 non-regression checks, and hosted CI verification.

## Baseline and branch

Work started from `origin/main` at `86b924b4b49d69c70c1688a8748cb1c2893adc34`
(merge commit of PR #1 completing Phase 1 and Phase 1.5) on feature branch
`feat/phase-2-component-model`. Before any source edit, the existing project built
cleanly and passed 3/3 CTest suites (`unit` with 92 checks, `backend_errors`, and
real `ngspice_rc` integration with 0 skips).

## Module layout and boundaries

Phase 2 adds a standalone C++20 static library target `datasim_component` in
`namespace datasim::component`, with zero third-party dependencies and zero link or
header dependency on `src/datasim/simulation/`, `<filesystem>`, process execution,
or ngspice:

| Path | Public types and functions | Responsibility |
| --- | --- | --- |
| `src/datasim/component/identity/ids.hpp` | `StrongId<Tag>`, `ComponentId`, `SourceDocumentId`, `EvidenceId`, `ObservationId`, `ReviewDecisionId`, `NormalizedParameterId`, `ComponentKnowledgeId`, `ModelArtifactId`, `ModelValidationId` | Compile-time distinct identifier types preventing accidental cross-entity ID substitution. |
| `src/datasim/component/identity/component_identity.hpp` | `ComponentFamily`, `ComponentIdentity` | Exact manufacturer, part number, preserved suffix, package, family/subtype, and synthetic flag. |
| `src/datasim/component/evidence/source_document.hpp` | `DocumentUsageRights`, `SourceDocument` | Immutable document metadata, revision, part associations, URL/retrieval date, optional SHA-256, and redistribution/license restrictions. |
| `src/datasim/component/evidence/evidence_location.hpp` | `EvidenceLocation` | Document provenance locator keeping 0-based `pdf_page_index` independent of `printed_page_label`, plus section, table/figure, cell/region, and footnotes. |
| `src/datasim/component/observation/operating_condition.hpp` | `OperatingCondition` | Test/operating conditions preserving original text, finite optional scalar, and mandatory unit symbol. |
| `src/datasim/component/observation/parameter_observation.hpp` | `ValueQualifier`, `ParameterCategory`, `ExtractionMethod`, `ParameterObservation` | Single-qualifier candidate reading preserving original text/notation, unit, optional finite value (absent stays `std::nullopt`), conditions, evidence refs, extraction provenance, and review audit history. |
| `src/datasim/component/observation/review.hpp`, `.cpp` | `ReviewState`, `ActorKind`, `ReviewerIdentity`, `ReviewDecision`, `is_valid_review_transition`, `requires_human_engineer_reviewer`, `apply_review_decision` | Deterministic engineering review state machine with strong exception safety and anti-self-approval enforcement. |
| `src/datasim/component/knowledge/normalized_parameter.hpp` | `NormalizedParameter`, `make_normalized_parameter` | Canonical SI parameter representation requiring an explicit `conversion_rule` while retaining `source_observation_id`, original text/unit, qualifier, and rating category. |
| `src/datasim/component/knowledge/component_knowledge.hpp`, `.cpp` | `ConflictResolutionStatus`, `ParameterConflict`, `KnowledgeRevisionEntry`, `ComponentKnowledge`, `promote_parameter_to_knowledge`, `create_next_knowledge_revision` | Versioned reviewed component facts, visible competing-observation records, silent-overwrite prevention, and sequential revision lineage. |
| `src/datasim/component/model/model_artifact.hpp` | `ModelFormat`, `AnalysisType`, `TerminalDescriptor`, `ModelParameterMapping`, `NumericRangeConstraint`, `ModelValidityEnvelope`, `ModelLicenseMetadata`, `ModelArtifactDescriptor` | Model metadata contracts: version, format, vendor/license, external URI/SHA-256, 1..N ordered pin mapping, parameter bindings, validity envelope, supported analyses, and limitations. |
| `src/datasim/component/model/model_validation.hpp`, `.cpp` | `ModelValidationStatus`, `ModelValidationKind`, `ModelValidationRecord`, `effective_model_validation_status` | Model validation records evaluated independently of observation approval, knowledge approval, and model registration. |
| `src/datasim/component/validation/domain_validation.hpp`, `.cpp` | `ValidationRuleCode`, `ValidationIssue`, `ValidationReport`, `ComponentDomainBundle`, `validate_*`, `validate_domain_bundle`, `ensure_valid`, `are_parameter_units_compatible`, `is_source_document_applicable`, `validate_model_parameter_mapping` | Deterministic per-entity structural checks, exact source document applicability, model parameter unit compatibility contracts, and cross-entity referential integrity rules. |

## Separation of conceptual, implemented, and deferred scope

| Capability | Phase 2 status | Scope detail |
| --- | --- | --- |
| Domain entities & strong IDs | IMPLEMENTED & VERIFIED | In-memory C++20 value structs and `StrongId<Tag>` types for scalar/symbolic observations, knowledge, and model descriptors. |
| Engineering review lifecycle | IMPLEMENTED & VERIFIED | Deterministic transitions `candidate -> needs_review -> approved \| rejected` and `approved -> superseded`; human-engineer gate and self-approval block. |
| Structural & referential validation | IMPLEMENTED & VERIFIED | 42 `ValidationRuleCode` diagnostics covering required fields, finite numbers, mandatory units, resolving IDs, conflict visibility, pin order, validity ranges, 4-way trust separation, parameter unit consistency, and exact source document part number applicability. |
| Synthetic passive fixtures | IMPLEMENTED & VERIFIED | Synthetic Resistor (`SYN-TF0603-1K00-LF`) and Capacitor (`SYN-C0G0805-1U0-J50NT`) bundles in `tests/unit/component_fixtures.*`. |
| Digitized curve series | PROPOSED (deferred) | Conceptual requirements remain in [COMPONENT_DATA_MODEL.md](COMPONENT_DATA_MODEL.md); figure locators exist in `EvidenceLocation`, but curve sample arrays are not yet modeled. |
| Unit conversion algebra | PROPOSED (deferred) | `NormalizedParameter` records explicit `conversion_rule` text; no automatic unit parser or dimensional solver is implemented. |
| Persistence & serialization | PROPOSED (deferred) | No database, file schema, or general-purpose JSON serializer is implemented for component domain types. |
| Datasheet ingestion / OCR / LLM | PROPOSED (deferred) | No PDF parser, OCR pipeline, or Ollama/LLM integration exists. |
| Knowledge-driven simulation | PROPOSED (deferred) | Phase 1/1.5 `RcConfig` and `ngspice` execution remain decoupled from `datasim_component`. |

## Build and test commands

From the repository root in PowerShell on Windows x64 (MSYS2 UCRT GCC 16.2.0):

```powershell
cmake -S . -B build/phase2 -G Ninja -DCMAKE_BUILD_TYPE=Release -DDATASIM_NGSPICE=D:/projects/datasim/build/tools/msys2-ngspice/ucrt64/bin/ngspice_con.exe
cmake --build build/phase2
ctest --test-dir build/phase2 --output-on-failure -V
```

Unit tests alone (requiring no external `ngspice` binary):

```powershell
ctest --test-dir build/phase2 -L unit --output-on-failure -V
```


## Domain validation hardening (pre-PR)

Before finalizing Phase 2, two domain validation mechanisms were hardened:

1. **Model parameter unit consistency**:
   - `are_parameter_units_compatible(expected_unit, param, mapping_expression)` validates unit
     compatibility whenever a model parameter mapping binds a `knowledge_param_id`.
   - Exact unit matches (`ohm` vs `ohm`, `F` vs `F`) are always accepted.
   - For `unitless` parameters (`param.unitless = true`), only unitless tokens (`1`, `unitless`,
     `dimensionless`, `-`) are accepted.
   - For differing units within the same physical dimension (e.g., `kohm` vs `ohm`, `degC` vs `K`),
     an explicit conversion contract in `mapping_expression` is required (e.g. `R / 1000.0`).
     Unspecified or automatic conversions without explicit contract are rejected.
   - Incompatible physical dimensions (e.g. resistance `ohm` vs capacitance `F`, voltage `V` vs
     current `A`) are strictly rejected with `ValidationRuleCode::incompatible_parameter_units`.
   - General-purpose algebraic unit conversion engines remain out of scope; original notation
     and `conversion_rule` remain fully preserved.

2. **Source document component association**:
   - `is_source_document_applicable(document, component)` verifies that every source document
     in a bundle is directly applicable to `ComponentIdentity::part_number`.
   - Exact part number matching is required: manufacturer part-number suffixes (e.g. `-LF`, `-J50NT`)
     are strictly preserved. Suffix-stripped or truncated part numbers are rejected.
   - Prefix matching is strictly prohibited: series-only prefixes (e.g. `SYN-TF0603` matching
     `SYN-TF0603-1K00-LF`) are not accepted.
   - Multi-part family documents (datasheets covering an entire product series with multiple
     associated part numbers) are explicitly supported: the document is applicable if and only if
     the component's exact MPN matches at least one entry in `associated_part_numbers`.
   - `validate_source_document` additionally detects and rejects duplicate entries in
     `associated_part_numbers` with `ValidationRuleCode::duplicate_identifier`.
   - Documents in a bundle not applicable to the bundle component trigger
     `ValidationRuleCode::incompatible_source_document_part_number`.

## Design limitations discovered during implementation

1. **In-memory lifetime only**: `ComponentDomainBundle` validates a single
   component's records in memory; cross-component shared-document catalogs and
   persistent storage transactions are deferred.
2. **Unverified conversion arithmetic**: Because general-purpose unit conversion is
   out of scope, validation confirms that `conversion_rule` and `normalized_si_unit`
   are explicitly provided and finite, not that `1.000 kohm * 1000 == 1000.0 ohm`
   dimensionally.
3. **Exact condition-tuple grouping**: Conflict and reversed-bound checks compare
   canonicalized `(parameter_name, original_value_text, unit_symbol, notes)` tuples
   independently of condition order, but do not evaluate continuous interval
   inclusion across differently phrased condition ranges.
4. **One-to-one terminal-to-pin bijection**: `TerminalDescriptor` currently rejects
   duplicate `physical_pin_id` values; multi-pin bonded packages tying several
   package pins to one electrical terminal will require an explicit pin-group list
   in a future model revision.
5. **Single-component bundle scope**: `ModelValidationRecord` currently links
   `EvidenceId` entries within the component bundle; referencing external simulation
   run manifests or physical lab datasets will require run-artifact URI/ID links
   when Stage 4 and Stage 6 are authorized.

Related: [Phase 2 plan](PHASE_2_PLAN.md),
[Phase 2 validation](PHASE_2_VALIDATION.md),
[ADR-0003](decisions/ADR-0003-component-domain-model.md),
[component data model](COMPONENT_DATA_MODEL.md).
