# Phase 4I implementation

Status: IMPLEMENTED. Decision:
[ADR-0010](decisions/ADR-0010-baseline-aware-text-reconstruction.md).
Verification: [PHASE_4I_VALIDATION.md](PHASE_4I_VALIDATION.md).

## Contract and algorithm

The public PDF-only additions are `ScriptPosition`, `BaselineScriptToken`,
`BaselineTextBinding`, `BaselineTextProvenance`,
`ElectricalTableLayout::baseline_text_bindings`, and
`ExtractionResult::baseline_text_provenance`. No intake, component, vertical, or
simulation contract changes. Empty bindings are the default.

Each binding selects one complete host word and exact source text plus explicitly
selected subscript/superscript words. The adapter verifies finite source boxes,
exact text, same-column containment, role direction, relative glyph height,
horizontal adjacency, unique eligible host, unique ownership, and no unrelated
intersecting text. Numeric bounds are defined in ADR-0010. They are conservative
word-box constraints, not measured font baselines or a general typography model.
Only ASCII alphanumeric/sign/decimal-point script text is currently supported.

Original `Word` coordinates remain intact. Verified tokens receive the host's
sort-y and center only for row grouping; the existing `row_y_tolerance` is never
changed. Text order remains the source x order. Multiple scripts on one host are
sorted deterministically. Conflicting bindings and unused expressions fail closed.
Parent-source/header overlap is unsupported; a reconstructed child expression can
still participate in the Phase 4H exact-text association.

Source strings remain space-joined tokens (`P 70`, `10 -4`). The provenance
sidecar separately records `P_{70}` or `10^{-4}` for explicit display semantics,
each original host/script word box, source text, role, and a separate evidence ID
linked by each affected observation. Configured selector boxes may be wider than
the actual words; provenance stores the actual word boxes. This is not verbatim
PDF byte preservation or source authentication.

Numerical parsing is unchanged: a reconstructed exponent stays multi-token and
unparsed. Inline value/unit bindings may still reject that representation. The
intra-value-cell overlap rule permits only a source-verified host/script pair's
bounded kerning overlap; unrelated word collisions remain errors. No approval,
normalization, model generation, or simulation occurs inside the PDF adapter.

## CLI and evaluation

Both CLIs accept repeatable `--baseline-text` with this syntax:

```text
HOST_BOX|HOST|sub/super|SCRIPT_BOX|SCRIPT[|sub/super|SCRIPT_BOX|SCRIPT...]
```

Boxes are `x_min,y_min,x_max,y_max` in top-origin Poppler points. For example:

```text
--baseline-text '24,116,34,128|P|sub|34,125,42,133|70'
```

The corpus manifest supports `table_geometry.baseline_text_bindings`, each with
`host_box`, `expected_host_text`, and `scripts` containing `source_box`,
`expected_text`, and `position` (`sub` or `super`). CLI text cannot contain `|`.
The C++ API does not depend on this CLI delimiter restriction.

The dump serializes `baseline_text_provenance`. The offline evaluator checks
configured coverage, actual-box containment, raw text, script roles, stable order,
display notation, and page/table/document/evidence references. New synthetic tests
also compare exact original coordinates and deliberately corrupt these fields.

Enabled reconstruction can change row indices when previously separated glyphs
coalesce. Existing annotations are not rewritten to hide that effect. The real
15/22 comparisons retain their earlier 3.5-point row tolerance and therefore keep
the same location keys; they test non-regression and new provenance, not an F1
improvement. New synthetic fixtures demonstrate coalescing at the unchanged
default 2-point tolerance.
