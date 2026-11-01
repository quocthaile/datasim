# Component data model

Status: Conceptual contracts across all seven entities below; **Phase 2 in-memory
C++20 contracts (`datasim_component`)** and the **Phase 3 deterministic ideal-RC
vertical slice (`datasim_vertical`)** are **IMPLEMENTED and VERIFIED** on Windows x64
(MSYS2 UCRT GCC 16.2.0 / ngspice-47) for entities 1–6 using synthetic and
engineering-reviewed Resistor and Capacitor fixtures. See
[Phase 2 implementation](PHASE_2_IMPLEMENTATION.md),
[Phase 2 validation](PHASE_2_VALIDATION.md),
[ADR-0003](decisions/ADR-0003-component-domain-model.md),
[Phase 3 implementation](PHASE_3_IMPLEMENTATION.md),
[Phase 3 validation](PHASE_3_VALIDATION.md), and
[ADR-0004](decisions/ADR-0004-phase-3-vertical-slice.md).

Persistent database schemas, general-purpose JSON serialization, digitized curve
series, automatic unit conversion, external SPICE subcircuit loading, and
visualization (entity 7) remain **PROPOSED** (deferred).

## Entity contracts

| Entity | Required conceptual information | C++20 status (`datasim::component` / `datasim::vertical`) |
| --- | --- | --- |
| Source document / evidence | Stable ID, publisher, part identity, document title and revision, original URL, retrieval date, content hash where available, rights restrictions; page index and printed page label, section, table/figure, cell or region and relevant footnotes. | **VERIFIED** in memory: `SourceDocument`, `DocumentUsageRights`, `EvidenceLocation` (`pdf_page_index` kept distinct from `printed_page_label`). |
| Parameter observation | ID, source/evidence references, original parameter name and notation, original value and units, minimum/typical/maximum qualifier or explicitly unspecified, conditions, rating category, extraction method/version, extraction time, uncertainty, and review status/history. | **VERIFIED** in memory for scalar/symbolic readings: `ParameterObservation`, `OperatingCondition`, `ValueQualifier`, `ParameterCategory`, `ExtractionMethod`, `ReviewDecision`. Digitized curve sample arrays remain PROPOSED. |
| Component knowledge | Component identity including manufacturer, part suffix and package; versioned normalized facts, canonical quantity/unit, explicit conversion, observation references, applicable conditions, conflicts, reviewer and decision rationale. | **VERIFIED** in memory: `ComponentIdentity`, `NormalizedParameter`, `ParameterConflict`, `KnowledgeRevisionEntry`, `ComponentKnowledge`. Automatic unit algebra remains PROPOSED. |
| Model artifact | ID/version/hash, format or equations, vendor/author, source and license, terminals/pin mapping, parameters and knowledge references, calibration/validation evidence, assumptions, supported analyses/backends, validity envelope and limitations. | **VERIFIED** metadata and ideal-RC compatibility gate (`ModelArtifactDescriptor`, `ModelValidityEnvelope`, `ModelValidationRecord`, `evaluate_rc_model_compatibility`). External subcircuit loading remains PROPOSED. |
| Circuit / run configuration | Topology, node and pin mappings, exact model versions/hashes, component values, stimuli, temperature, initial conditions, analysis and sweep settings, solver tolerances/options, backend/version, platform and random seeds if relevant. | **VERIFIED** for ideal-RC vertical slice (`CircuitAssembly`, `CircuitComponentInstance`, `TerminalNodeBinding`, `RcRunInputSpecification`, `AdaptedRcSimulation`, `RcConfig`). Arbitrary topologies remain PROPOSED. |
| Simulation result | Run/configuration reference, execution manifest, completion status, independent axes and units, dependent arrays and units, diagnostics, convergence warnings, timestamps, backend output reference/hash, and provenance. | **VERIFIED** for ideal-RC vertical slice (`VerticalRcRunResult`, `VerticalSliceProvenanceManifest`, `provenance_metadata.json`, `run_metadata.json`, `validation_report.json`, CSVs). General multi-analysis result model remains PROPOSED. |
| Visualization / explanation | Result and evidence references, view configuration, units, transformations/interpolation, annotations, assumptions, generation method/version, and review history where applicable. | **PROPOSED** (not implemented). |

## Preservation and normalization

Keep minimum, typical, and maximum as distinct observations or explicitly labeled
values under the same conditions. In `datasim::component`, each
`ParameterObservation` carries a single `ValueQualifier` (`minimum`, `typical`,
`maximum`, or `unspecified`) so bounds cannot be silently combined. A blank
datasheet cell means absent (`std::nullopt`), not zero. Preserve inequalities,
ranges, symbols, tolerances, test circuits, pulse duration, temperature, frequency,
and footnotes. Unit conversion creates a derived `NormalizedParameter` with a
documented `conversion_rule`; it never replaces `preserved_original_value_text` or
`preserved_original_unit_symbol`.

Use distinct categories (`ParameterCategory`) for `absolute_maximum_rating`,
`recommended_operating_condition`, `electrical_characteristic`,
`thermal_characteristic`, and `other_unspecified`. Absolute maxima are stress
limits, not operating targets or guaranteed model parameters;
`validate_domain_bundle` deterministically rejects behavioral
`ModelParameterMapping` bindings to `absolute_maximum_rating` parameters. Never
infer category or model applicability from the parameter name alone.

Curve observations additionally need figure location, axis units, linear/log scale,
calibration points, trace/legend identity, digitization method, and uncertainty.
Keep curve samples separate from fitted model coefficients. Typical curves do not
establish worst-case bounds. Datasheet observations are published evidence, not
measurements performed by Datasim. (Phase 2 implements figure locators on
`EvidenceLocation` while deferring curve sample series.)

## Promotion and revision

Observation review states (`ReviewState`) are `candidate`, `needs_review`,
`approved`, `rejected`, and `superseded`. They are distinct from project capability
statuses. `apply_review_decision` and `validate_parameter_observation` enforce the
deterministic state machine:

```text
candidate -> needs_review -> approved | rejected
approved  -> superseded (requires superseding_observation_id)
```

Candidates pass structured intake checks and then an identified human engineer
(`ActorKind::human_engineer`) reviews evidence, units, qualifiers, conditions,
category, and intended use. `ActorKind::llm_assistant` and
`ActorKind::automatic_parser` are prohibited from issuing review decisions, and
self-approval matching `extraction_tool` is rejected. Only `approved` observations
may contribute to `ComponentKnowledge`. Approval of a fact does not validate a
model: `effective_model_validation_status` evaluates `ModelValidationRecord`
entries independently of observation and knowledge approval.

Retain competing observations in `ParameterConflict` rather than silently averaging
or overwriting conflicts. Unresolved conflicts block promotion of competing
observations while keeping them visible; `resolved_by_review` conflicts require a
human engineer and rationale and promote only the selected observation. Record
reviewer, timestamp, rationale, and `previous_version_id` in `version_history` for
every knowledge revision.

## Illustrative lineage

A hypothetical table entry `10 mA`, qualified as typical at `25 degC`, remains a
typical observation at that condition. Normalization may derive `0.010 A` while
retaining the original text and table location. It cannot become a maximum current
rating or a complete device model. Even after review, a separate model decision
must establish what equation, terminal, and validity range can use that value.
This example and the Phase 2 Resistor/Capacitor test fixtures are synthetic and
describe no measured device.

See [validation](VALIDATION_STRATEGY.md), [architecture](ARCHITECTURE.md), and
[Phase 2 implementation](PHASE_2_IMPLEMENTATION.md).
