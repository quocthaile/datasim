# Phase 4G Plan: Deterministic Inline Value/Unit Parsing

## Status

- **Phase**: Phase 4G
- **Branch**: `feat/phase-4g-inline-value-unit-parsing`
- **Baseline**: `main` at `9f3c115d13a40204d3d7f83382a50c107b14b803` (PR #10 merged, hosted Windows CI run `42044651168` passed)
- **ADR**: [ADR-0008](decisions/ADR-0008-inline-value-unit-parsing.md)

## Objective

Implement Phase 4G as a bounded, opt-in extension to `datasim_pdf` that supports explicitly configured inline numeric value and unit tokens within a single PDF table value cell (`minimum_column`, `typical_column`, `maximum_column`).

This phase addresses **P2 (Inline Value + Unit Single-Cell Encoding)** from the Phase 4E real-datasheet evaluation findings ([`PHASE_4E_VALIDATION.md`](PHASE_4E_VALIDATION.md)), while preserving the existing five-library architecture, all `datasim_intake` and `datasim_component` contracts, and the `ReviewState::candidate` boundary.

## Scope

### In Scope

1. **Explicit opt-in configuration (`InlineValueUnitBinding`)** on `ElectricalTableLayout` (`src/datasim/pdf/table_extraction.hpp`), binding a `ValueQualifier` (`minimum`, `typical`, `maximum`) to an explicit allowlist of valid source unit tokens (`allowed_unit_symbols`).
2. **Source-cell provenance (`ValueCellProvenance`)** on `ExtractionResult`, preserving the complete verbatim source cell text (`raw_cell_text`, e.g., `"200 V"`), decomposed numeric token (`raw_value_token`, e.g., `"200"`), and decomposed inline unit token (`raw_inline_unit_token`, e.g., `"V"`), linked by `observation_id` and `value_evidence_id`.
3. **Bounded token parsing** for configured inline value/unit cells:
   - Strict numeric scalar token (`intake::parse_unambiguous_scalar()`), footnote-marked scalar (`"200* V"`, `"200 V*"`), or locale-ambiguous comma number (`"1,000 V"`).
   - Whitespace-delimited unit token verified against `allowed_unit_symbols` with exact spelling preservation (including Unicode symbols such as `µF` / `μF`).
   - Fail-closed rejection of multi-token cells, unpermitted unit suffixes, ranges (`"0.22..0.47 uF"`), tolerances (`"±5 %"`), and undelimited strings.
4. **Deterministic unit precedence and conflict resolution** across inline cell units, explicit row `unit_column` cells, and Phase 4F `HeaderUnitBinding` entries.
5. **Intra-cell bounding-box collision safety**: reject overlapping or duplicate Poppler `<word>` bounding boxes inside any value cell while preserving existing column-level non-overlap and word-containment rules without weakening.
6. **CLI and offline evaluator wiring** in `datasheet_extract`, `datasheet_eval_dump`, and `scripts/eval/evaluate_pdf_corpus.py`.
7. **Synthetic fixtures and regression tests** (`tests/fixtures/synthetic_inline_unit_datasheet.pdf`, `pdf_extraction_tests`, and `test_pdf_corpus_evaluator.py`) covering all 14 required cases without altering historical Phase 4C–4F fixture checksums.
8. **Real-corpus re-evaluation** across Phase 4E, Phase 4F, and Phase 4G baselines (separating identical 15-observation subset metrics from 22-observation P2-expanded sub-band metrics).

### Out of Scope

- Merged-cell handling, multi-MPN column demultiplexing, or hierarchical parent-row context (**P3**).
- Automatic unit inference from parameter names or electrical conventions.
- Implicit unit scaling or conversion (e.g., `nF` to `pF`, `mV` to `V`).
- Changes to `datasim`, `datasim_component`, `datasim_vertical`, or `datasim_intake`.
- OCR, LLM integration, persistence, GUI, semiconductor models, or custom solvers.

