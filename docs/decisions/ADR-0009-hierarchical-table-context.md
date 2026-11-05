# ADR-0009: Explicit hierarchical table context

Date: 2026-11-05. Status: Accepted for Phase 4H implementation.

## Context

The Phase 4G parser groups selected words into physical rows before extracting
parameter/value cells. Its row numbers also key the existing offline evaluator.
`EvidenceLocation` has page/table/region locators, but no bounding-box or excerpt
fields. Neither it nor the candidate-only intake contract needs modification.

## Decision

Append `parent_context_bindings` to `ElectricalTableLayout`. Each
`ParentContextBinding` contains a `SourceBox parent_box` (top-origin Poppler points),
`expected_parent_text`, and explicit `ParentContextChild` selections with a
one-based physical `row_index` and `expected_parameter_text`. Empty child text is
allowed only for bounded merged parameter cells. No caller-supplied replacement
label, implicit range expansion, semantic classification, or indentation heuristic
is introduced. Parent text plus one space plus nonempty child text forms the
candidate name; a blank child inherits the parent alone. Punctuation is unchanged.

Validate finite, positive-area parent boxes wholly inside the table parameter
column and outside the configured header. Reject intersecting parent boxes and
duplicate child assignments. Verify every intersecting source word is wholly
contained; require nonempty exact whitespace-joined text, one baseline within the
existing row tolerance, and no overlapping word boxes. Preserve original row
grouping and numbering, including parent-only rows. Remove verified parent words
only from parameter-cell reading, never from physical row segmentation.

Every target must exist, contain a non-placeholder value cell, and match its exact
expected child text after removing parent words. A heading must end above its
children. A vertically merged box may overlap child rows only when their remaining
parameter cells are empty and their entire row height lies inside the box. Such
merged targets must be consecutive physical rows; every value-bearing row touched
by the parent box must be assigned to it. Reject partial row intersections,
competing parameter text, unassigned value rows, and any conflicting assignments.
All failures return no manifest, bundle, or provenance. No arbitrary spanning
grids or multi-MPN association is added; existing explicit single-component column
selection and geometry checks remain required.

Append `ParameterContextProvenance` to `ExtractionResult`, one entry per affected
observation: observation/parent/child evidence IDs, row index, verified parent box,
child row word-union box, raw parent text, raw child parameter text, and full raw
child row text. Existing `EvidenceLocation` references remain the durable link in
the candidate bundle; the geometry/text sidecar must accompany it for detailed
review. Parent evidence is separate from original child evidence. Existing IDs and
value-cell provenance are retained. No approval, normalization, or simulation runs.

## Alternatives and limits

Reject proximity inference and a global row tolerance change: both could attach
neighboring parameters. Reject arbitrary replacement labels: source verification
must constrain composition. Reject domain/intake API changes: PDF geometry belongs
to the adapter. A list of exact physical rows is smaller and safer than a general
region-to-table model, although callers must update selections when geometry
changes. Baseline-shifted text reconstruction remains Phase 4I, not this decision.
Without bindings the existing extraction path is unchanged.

Real-vendor bindings and annotations stay under ignored `build/`; public fixtures
are fictional. Poppler remains an external executable dependency, is not bundled,
and no project license or redistribution decision is adopted.
