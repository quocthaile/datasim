# Phase 4H implementation

Status: IMPLEMENTED on `feat/phase-4h-hierarchical-table-context`.
Decision: [ADR-0009](decisions/ADR-0009-hierarchical-table-context.md).
Evidence: [validation](PHASE_4H_VALIDATION.md).

## Public contract additions

Only [datasim_pdf](../src/datasim/pdf/table_extraction.hpp) gains public types:
`SourceBox`, `ParentContextChild`, `ParentContextBinding`, and
`ParameterContextProvenance`. The appended vectors are
`ElectricalTableLayout::parent_context_bindings` and
`ExtractionResult::parameter_context_provenance`. Default empty bindings preserve
the old extraction path. All existing identifiers and downstream APIs are retained.

`ParentContextChild` selects a one-based physical row, including parent-only rows,
and its exact expected parameter text. `SourceBox` uses Poppler top-origin points.
The binding supplies exact parent text and its box, confined to the parameter
column. A candidate label is the verified parent followed by a single space and
the nonempty original child label. Blank children inherit the parent alone.
No punctuation, symbols, units, numbers, or engineering meaning are normalized.

## Verification and association

1. Preserve Phase 4G row grouping, tolerance, ordering, and observation/value IDs.
2. Verify finite nonempty parent boxes in the selected non-header parameter region.
   Reject intersecting parent boxes, clipped words, overlapping text, multiple
   baselines, missing text, and exact-text mismatches.
3. Verify every selected child exists, contains a value, has the expected remaining
   parameter text, and has exactly one parent. Verify heading order and reject
   crossing an intervening parameter-only heading, even if child text matches.
4. A parent box overlapping data rows is supported only for consecutive, fully
   bounded child rows with no competing parameter text. Every value row touched
   by that box must be explicitly assigned. General merged grids remain unsupported.
5. Remove verified parent words from parameter-cell reading, never from physical
   row grouping. Unselected siblings and complete neighboring labels stay local.
6. Emit separate parent and child `EvidenceLocation` references and a per-observation
   sidecar holding both boxes, original parent/child text, and full child-row text.
   Text is the existing deterministic, space-joined Poppler word representation,
   not a claim to preserve PDF text-stream bytes or layout typography.

Association failures are atomic: no manifest, candidate bundle, or sidecars escape.
Successful output still passes the existing intake builder, remains candidate-only,
and cannot produce approved knowledge, models, or simulation execution.

## CLI and evaluation

Both extraction CLIs accept repeatable:

```text
--parent-context '20,115,210,130|Pulse rating:|2:short|3:long'
```

Syntax is `x_min,y_min,x_max,y_max|PARENT|ROW:CHILD[|ROW:CHILD...]`.
Use an empty child after the colon for a bounded merged cell. Pipe characters in
source strings are unsupported by this CLI encoding (the C++ API has no such
restriction). Numeric fields are parsed strictly. Different bindings must have
different, non-overlapping boxes; duplicate flags are not silently merged.

The corpus manifest's optional `table_geometry.parent_context_bindings` uses
`parent_box: [x_min,y_min,x_max,y_max]`, `expected_parent_text`, and a `children`
array of `row_index` / `expected_parameter_text`. The dump emits the sidecar, and
the evaluator's evidence agreement checks its IDs, parent box, text, row index,
nondegenerate child box, page/table/document locators, and observation references.
Synthetic tests additionally check child text and coordinate bounds against the
fixture specification, and reject corrupted provenance. These are traceability
checks, not authentication of source metadata or human review.

Real-vendor configurations containing exact text remain in ignored `build/`.
The public fixture and expected labels contain fictional values only. No new
dependency is added; Poppler is neither linked nor bundled. Licensing and
redistribution decisions remain with the owner.
