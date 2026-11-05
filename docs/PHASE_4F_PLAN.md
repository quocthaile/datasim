# Phase 4F Plan — Explicit Column-Header Unit and Condition Binding

- **Phase**: 4F
- **Status**: IMPLEMENTED and VERIFIED on `feat/phase-4f-explicit-header-binding` (synthetic regression suite A–H verified in CTest; Phase 4E pilot corpus re-evaluated locally against untracked manufacturer PDFs in `build/corpus/`)
- **Baseline**: `main` at `b45c32224e0316dd9f93b6e9bfc3ab89cd3ceaf4` (PR #9 merge of Phase 4E, verified by hosted Windows CI run [`42037281172`](https://github.com/quocthaile/datasim/actions/runs/42037281172))
- **Related Documents**:
  - [ADR-0007: Explicit Column-Header Unit and Condition Binding](decisions/ADR-0007-explicit-header-binding.md)
  - [Phase 4F Implementation](PHASE_4F_IMPLEMENTATION.md)
  - [Phase 4F Validation](PHASE_4F_VALIDATION.md)
  - [Phase 4E Validation](PHASE_4E_VALIDATION.md)

## 1. Objective

Phase 4E established a reproducible offline benchmark for `datasim_pdf::extract_electrical_table()` and demonstrated that on real manufacturer datasheets:

1. Five of the six observation-level mismatches (`REAL-CASE-07-150D-RATINGS-SUBBAND` in `vishay_150d_tantalum.pdf`) were caused by the capacitance unit `(μF)` being declared in the column header (`CAPACITANCE AT +25 °C 120 Hz (μF)`) rather than repeated in each row's `unit_column` cell.
2. Five unresolved operating-condition warnings (`REAL-CASE-05-VJ-MLCC-C0G-MIN-BAND` in `vishay_vj_commercial_mlcc.pdf`) were caused by the condition quantity and unit (`MAXIMUM VOLTAGE (V)`) being declared in the column header while rows contained bare numbers (`"100"`, `"250"`, `"500"`, `"630"`).

Phase 4F implements the **P1** recommendation from Phase 4E ([ADR-0007](decisions/ADR-0007-explicit-header-binding.md)): an optional, caller-configured, source-verified column-header unit and condition binding mechanism confined strictly within `datasim_pdf`.

## 2. Scope and Boundaries

1. **Isolated to `datasim_pdf`**:
   - No changes to `datasim`, `datasim_component`, `datasim_vertical`, or `datasim_intake`.
   - Existing `ExtractionRequest` configurations with empty `header_unit_bindings` and `header_condition_binding == std::nullopt` behave identically to Phase 4C–4E.
2. **Explicit Verification Only (No Heuristic Inference)**:
   - Every `HeaderUnitBinding` and `HeaderConditionBinding` must specify an explicit 2D bounding box inside `[table.region_x_min, table.region_x_max] x [table.header_y_min, table.header_y_max]` overlapping the target column, the exact expected header text, and the exact unit symbol (plus `parameter_name` for condition headers) appearing in that verified header text.
   - Missing, mismatched, out-of-region, non-overlapping, or straddling header boxes fail closed.
   - Row/header unit conflicts and inline/header condition conflicts fail closed.
   - Non-numeric condition cells (such as package/case code `"A"` in `150D`) remain unresolved with `raw_conditions_text` preserved and a diagnostic emitted.
3. **Separate Header Provenance**:
   - Every observation that relies on a header unit binding attaches a dedicated `EvidenceLocation` (`cell_or_region = "header_unit;column=<min|typ|max>"`) to `observation.evidence_refs`.
   - Every observation that relies on a header condition binding attaches a dedicated `EvidenceLocation` (`cell_or_region = "header_condition;column=condition"`) to `observation.evidence_refs`.
4. **Out of Scope (Explicit Non-Goals)**:
   - No automatic header detection, OCR, arbitrary merged-cell propagation (P3), inline value+unit cell splitting (P2), or subscript/superscript baseline coalescing (P4).
   - No automatic unit conversion (`μF` is preserved verbatim as `"μF"`, never converted to `F` or ASCII `"uF"` by the parser).
   - No automatic approval of extracted candidates.

## 3. Deliverables

1. **Architecture Decision Record**: [`docs/decisions/ADR-0007-explicit-header-binding.md`](decisions/ADR-0007-explicit-header-binding.md).
2. **`datasim_pdf` Extension**:
   - `HeaderUnitBinding` and `HeaderConditionBinding` in [`src/datasim/pdf/table_extraction.hpp`](../src/datasim/pdf/table_extraction.hpp).
   - Fail-closed validation, header text extraction, precedence/conflict handling, and header `EvidenceLocation` attachment in [`src/datasim/pdf/table_extraction.cpp`](../src/datasim/pdf/table_extraction.cpp).
3. **CLI & Evaluation Harness Support**:
   - Extend [`apps/datasheet_extract/main.cpp`](../apps/datasheet_extract/main.cpp), [`apps/datasheet_eval_dump/main.cpp`](../apps/datasheet_eval_dump/main.cpp), and [`scripts/eval/evaluate_pdf_corpus.py`](../scripts/eval/evaluate_pdf_corpus.py) to pass optional `header_unit_bindings` and `header_condition_binding` from manifest configurations and verify header evidence references.
4. **Synthetic Regression Fixtures (Cases A–H)**:
   - Extend [`tests/fixtures/make_synthetic_datasheet_pdf.py`](../tests/fixtures/make_synthetic_datasheet_pdf.py) to generate a dedicated synthetic PDF fixture `tests/fixtures/synthetic_header_binding_datasheet.pdf` (preserving `synthetic_datasheet.pdf` and `image_only_datasheet.pdf` byte-for-byte unchanged) and verify Cases A–H in [`tests/unit/pdf_extraction_tests.cpp`](../tests/unit/pdf_extraction_tests.cpp) and [`tests/evaluation/test_pdf_corpus_evaluator.py`](../tests/evaluation/test_pdf_corpus_evaluator.py).
5. **Phase 4E vs. Phase 4F Benchmark Comparison**:
   - Re-evaluate the real-datasheet pilot corpus (`evaluation/real_corpus_manifest.json` vs. `evaluation/real_corpus_manifest_phase4f.json`) and record the exact baseline vs. post-change metrics in [`docs/PHASE_4F_VALIDATION.md`](PHASE_4F_VALIDATION.md).
