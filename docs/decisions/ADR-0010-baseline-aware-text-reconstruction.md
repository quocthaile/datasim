# ADR-0010: Bounded baseline-aware text reconstruction

Date: 2026-11-05. Status: Accepted for Phase 4I implementation.

## Context and coordinates

Phase 4H represents Poppler words as `{x0,y0,x1,y1,text}` in top-origin PDF points.
Poppler word boxes do not expose typographic baselines. Row grouping sorts by y0
and compares vertical box centers against `row_y_tolerance`. Small, lowered or
raised glyphs can therefore form separate physical rows. Increasing the global
tolerance would also combine unrelated neighboring rows.

## Decision

Append opt-in `baseline_text_bindings` to `ElectricalTableLayout`. A
`BaselineTextBinding` selects one exact host word through `host_box` and
`expected_host_text`, plus one or more `BaselineScriptToken` selections, each with
`source_box`, exact `expected_text`, and explicit `ScriptPosition` (subscript or
superscript). Reuse `SourceBox`. No caller-supplied replacement text is accepted.

Each selection must contain exactly one complete word, match its expected text,
have finite positive geometry, and stay in one selected non-header data column.
The caller selects the role; geometry corroborates it, never discovers roles.
The host must be unique among page words satisfying the same constraints for each
script: script height at most 90% of host height, signed center displacement in
the requested direction with magnitude at most one host height, and horizontal
gap between -10% and +75% of host height. The small negative gap allows kerning of
individually reported glyph boxes, not arbitrary column overlap. Multiple script
tokens must have nonoverlapping horizontal boxes in x order. Reject multiple
possible hosts, reused words, duplicate/overlapping expression regions, unrelated
text intersecting the expression envelope, wrong direction, and conflicts with
header/parent-source boxes. Script strings contain only ASCII letters, digits,
signs, and decimal points; parentheses/asterisks and other footnote markers are
not reconstructed. This is intentionally narrower than general typography.

Before ordinary row grouping, give selected script words only the host's sorting
y0 and center for row association. Keep original coordinates and text unchanged.
Other words and the global row tolerance are untouched. The existing x ordering
still produces space-joined source text (for example `P 70` or `10 -4`). A separate
display expression uses explicit `_{...}` / `^{...}` markers, preserving the
distinction between subscripts and superscripts without claiming those markers
were literal PDF source text. Scripts are serialized in deterministic x order.

Raw `original_value_text` and `ValueCellProvenance` continue to contain space-joined
source tokens. Reconstructed numeric-looking expressions are not evaluated:
`10 -4` and `10 4` remain unparsed by the unchanged strict scalar grammar. Existing
inline-value/unit validation may reject such multi-token expressions; reconstruction
does not broaden that grammar. The existing intra-value-cell collision rejection
is waived only for a verified host/script pair, never unrelated words.

Append per-observation `BaselineTextProvenance` containing a separate evidence ID,
host and script original boxes/text/roles, reconstructed source text, and display
expression. Attach it only to observations using that parameter, condition, unit,
or value cell. Existing domain/intake APIs stay unchanged; all observations remain
candidates. Publish no partial result on reconstruction failure. Disabled bindings
must reproduce Phase 4H outputs and diagnostics. Enabled row coalescing may change
row indices; comparisons requiring unchanged location keys must report this
explicitly rather than rewrite historical ground truth.

## Scope and consequences

This is explicit word association, not a font interpreter, OCR, automatic symbol
recognition, numerical exponent parser, or general multi-axis table recognizer.
The bounding-box rules may reject valid typography; rejecting an uncertain pair
is preferable to silently attaching a nearby row or footnote. Source metadata is
not authenticated. Detailed provenance must accompany candidate output for review.

Synthetic fixtures cover ordinary text, both script roles, multiple scripts,
scientific notation, ambiguous hosts, adjacent rows, nearby headers/footnotes,
conflicting geometry, repeated extraction, and Phase 4H context composition.
Local vendor data and annotations remain ignored and unchanged. Phase 4J starts
only after this phase merges and hosted post-merge CI succeeds.
