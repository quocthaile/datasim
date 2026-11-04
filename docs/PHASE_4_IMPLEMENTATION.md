# Phase 4B implementation: deterministic evidence intake

Status: **IMPLEMENTED** on `feat/phase-4b-evidence-intake`. This document
describes the bounded in-memory intake implementation; it does not claim
datasheet extraction accuracy, source authenticity, or physical measurement.

## Scope

`datasim_intake` is a C++20 library in `namespace datasim::intake`. It links only
`datasim_component` and performs no file, network, PDF, OCR, persistence, LLM,
unit-conversion, or simulation operations.

The public contracts are defined in
[`src/datasim/intake/intake.hpp`](../src/datasim/intake/intake.hpp):

- `IntakeOriginClass` distinguishes only `synthetic_fixture` and
  `manual_curation`.
- `IntakeManifest`, `IntakeEvidenceEntry`, and `IntakeObservationEntry` compose
  existing component-domain records and retain raw qualifier/condition and
  ambiguity metadata.
- `IntakeDiagnosticCode`, `IntakeDiagnostic`, and `IntakeReport` distinguish
  hard errors from recoverable warnings.
- `validate_intake_manifest` validates the intake boundary.
- `build_candidate_bundle` returns an optional, fully validated candidate bundle.

## Implemented rules

The intake layer delegates structural and domain checks to the existing
validators, then adds exact manufacturer/publisher and part-number/suffix
checks, declared PDF page bounds, explicit page-index provenance, synthetic
marker consistency, candidate-only review state, qualifier/category consistency,
and strict raw numeric handling. Numeric values remain in their original unit;
no conversion is performed.

Ambiguous raw values remain candidates with a warning when no parsed value is
asserted. An asserted value for ambiguous text is a hard error. Hard errors
produce no bundle. Intake never creates review decisions, knowledge, normalized
parameters, models, or model-validation records.

## Tests

`intake` covers valid synthetic provenance, distinct PDF and printed page
labels, exact original-unit preservation, ambiguous values, asserted ambiguity,
page bounds, publisher mismatch, and candidate-only enforcement.
`intake_handoff` verifies domain validation and the empty knowledge/model
boundary. Fixtures use fictional synthetic identities and contain no vendor
material.

The existing `datasim`, `datasim_component`, and `datasim_vertical` sources are
unchanged.
