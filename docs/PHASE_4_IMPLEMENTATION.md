# Phase 4B implementation: deterministic evidence intake

Status: **IMPLEMENTED and VERIFIED** on `main` after PR #6 at
`7d9af1c88929fe3c29d55e1c2438e8d2041acb2a`, hosted run
[41995104973](https://github.com/quocthaile/datasim/actions/runs/41995104973).
This document describes the bounded
in-memory intake implementation; it does not claim datasheet extraction
accuracy, source authenticity, or physical measurement.

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
  ambiguity metadata while the manifest is retained by the caller.
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

When both raw text and a parsed value are supplied, a complete strict scalar is
parsed again and compared using exact IEEE-754 equality. This accepts `"1.000"`
with `1.0` and rejects a conflicting asserted value. Whitespace, ranges,
inequalities, tolerances, locale separators, and non-finite values are not
scalar literals.

Suffix validation checks only that the exact component part number ends with the
provided suffix and that the source contains that exact part number. It does
not infer a separator or reconstruct a manufacturer MPN.

Minimum/maximum checks compare only observations with the same parameter name,
original unit symbol, category, and complete operating-condition records.
Different temperatures or units are not compared, and duplicate conflicting
candidates receive a warning while remaining visible.

## Tests

`intake` covers valid synthetic provenance, distinct PDF and printed page
labels, exact original-unit preservation, raw/parsed consistency, ambiguous
values, asserted ambiguity, page bounds, publisher mismatch, suffix formats,
duplicate IDs, unresolved evidence, and candidate-only enforcement.
`intake_handoff` exercises the existing review, normalization, promotion, and
aggregate-validation APIs. Fixtures use fictional synthetic identities and
contain no vendor material.

The `ngspice_rc` integration now also constructs a synthetic candidate bundle,
performs the existing human review and normalization handoff, attaches the
existing validated synthetic model descriptors, and executes `datasim_vertical`.
This is an **IMPLEMENTED** release-gate test. Its numerical and byte-level
results are **VERIFIED** only when a real ngspice run completes without a CTest
skip; a skipped local integration remains **NOT VERIFIED**.

The row/column labels, raw qualifier/condition text, ambiguity notes, and
derived-page-index marker are intake-sidecar fields. They are not fields of
`ComponentDomainBundle`; `build_candidate_bundle` does not copy them into the
domain aggregate. `pdf_page_index`, `printed_page_label`, and `footnote_refs`
are copied through `EvidenceLocation`, while raw values, units, qualifiers,
categories, conditions, and evidence IDs are copied through
`ParameterObservation`. Callers must retain the original `IntakeManifest` if
sidecar metadata is needed after construction.

The existing `datasim`, `datasim_component`, and `datasim_vertical` sources are
unchanged.
