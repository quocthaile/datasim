# Validation strategy

Status: Engineering validation program with **Phase 1/1.5 ideal-RC numerical
verification** and **Phase 2 in-memory domain-contract verification** completed on
Windows x64 (MSYS2 UCRT GCC 16.2.0 / ngspice-47). Extraction benchmarks on real
datasheets, physical hardware measurements (`MEASURED`), and presentation-layer
checks remain **PROPOSED**.

See [Phase 1 and 1.5 validation](PHASE_1_VALIDATION.md),
[Phase 1.5 reproducibility](PHASE_1_REPRODUCIBILITY.md), and
[Phase 2 validation](PHASE_2_VALIDATION.md) for exact scope, commands, outcomes,
and limitations.

## Gates

| Gate | Checks | Evidence needed to pass | Current status |
| --- | --- | --- | --- |
| Source integrity | Identity, revision, hash, readable location, rights and footnotes; PDF index distinct from printed page label. | Traceable source manifest and permitted evidence access. | **VERIFIED** in memory on synthetic fixtures (`validate_source_document`, `validate_evidence_location`). Real document ingestion remains PROPOSED. |
| Extraction structure | Required fields, finite numeric values, explicit units, distinct qualifiers, explicit categories, conditions with units, and resolving source references. | Machine-check results; ambiguous/missing values remain `std::nullopt` and blocked from approval. | **VERIFIED** in memory (`validate_parameter_observation`, `validate_domain_bundle`). Automated parsers remain PROPOSED. |
| Engineering review | Compare original source; assess plausibility, conflicts, operating envelope and intended use; enforce transition graph. | Named `human_engineer` reviewer, decision, ISO-8601 date and rationale; deterministic rejection of LLM/parser self-approval. | **VERIFIED** in memory (`apply_review_decision`, `validate_review_decision`, `ParameterConflict` checks). |
| Model suitability | Equations/artifact URI, 1..N ordered pins, parameter mapping, validity envelope ranges, license, limitations, and independent validation records. | Independent model review and documented limitations; block `absolute_maximum_rating` as behavioral model parameter. | **VERIFIED** for metadata contracts (`validate_model_artifact_descriptor`, `effective_model_validation_status`). Model loading/execution remains PROPOSED. |
| Numerical verification | Analytic cases, reference simulations, common-grid convergence, repeatability and process-fault checks. | Reproducible inputs, versions, settings, tolerances and actual outcomes. | **VERIFIED** for Phase 1/1.5 manual ideal-RC transient (`0.05 V` tolerance, 4 refinements, 2 repeats, Windows local + hosted CI). Other circuits remain PROPOSED. |
| Physical validation | Compare predictions against controlled physical observations. | Instrument/setup/calibration, raw observations, conditions and uncertainty. | **PROPOSED** (no physical measurements acquired). |
| Presentation integrity | Axis units/scales, transformations, missing data, warnings and explanation citations. | Comparison against raw results and evidence. | **PROPOSED** (no UI/visualization implemented). |

## Test design principles

Use licensed or explicitly marked synthetic fixtures (`is_synthetic = true`).
Phase 2 unit tests (`tests/unit/component_domain_tests.cpp`, 231 checks) exercise
missing units, missing/blank values, non-finite numbers (`NaN`/`Inf`), reversed
min/max bounds, distinct PDF vs printed page labels, footnotes, absolute maximum
ratings vs recommended operating conditions, competing conflicting observations,
invalid review transitions, non-human/self-approval attempts, invalid pin mappings,
inverted validity envelopes, and model validation independence on synthetic Resistor
and Capacitor bundles.

Future extraction benchmarks must assess value, unit, qualifier, condition, and
provenance accuracy separately against engineer-reviewed labels rather than using a
single aggregate score that hides condition or source-identity errors.

Future numerical cases beyond the verified ideal-RC spike (such as a resistor
divider or reviewed nonlinear device model) must define expected behavior and
tolerances before execution, pin backend/model versions and input hashes, and fail
visibly on unsupported models, non-convergence, invalid topology, or out-of-envelope
use. Deterministic computation does not promise bitwise equality across platforms;
report observed reproducibility within declared tolerances and separate model error,
extraction error, numerical error, and measurement uncertainty.

## Documentation acceptance

Check all local Markdown targets, fenced-block balance, Mermaid parse validity,
bibliography keys and original identifiers, [charter status](PROJECT_CHARTER.md)
terminology, and cross-document agreement. Inspect Git status and diff for
unintended changes, and report exact executed commands and unverified platforms in
each phase validation record.
