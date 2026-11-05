# Datasim — Phase 4F Implementation: Explicit Header Unit and Condition Binding

## 1. Overview

Phase 4F implements the narrowly scoped, source-verified column-header unit and condition binding extension proposed in [ADR-0007](decisions/ADR-0007-explicit-header-binding.md) and planned in [`docs/PHASE_4F_PLAN.md`](PHASE_4F_PLAN.md).

All production changes are isolated inside `datasim_pdf`:

- [`src/datasim/pdf/table_extraction.hpp`](../src/datasim/pdf/table_extraction.hpp)
- [`src/datasim/pdf/table_extraction.cpp`](../src/datasim/pdf/table_extraction.cpp)

Zero changes were made to `datasim`, `datasim_component`, `datasim_vertical`, or `datasim_intake`.

---

## 2. Production Contracts in `datasim_pdf`

### 2.1 `HeaderUnitBinding` and `HeaderConditionBinding`

Added to [`src/datasim/pdf/table_extraction.hpp`](../src/datasim/pdf/table_extraction.hpp):

- **`datasim::pdf::HeaderUnitBinding`**:
  - `qualifier` (`component::ValueQualifier::minimum`, `typical`, or `maximum`)
  - `header_x_span` (`HorizontalSpan`)
  - `header_y_min`, `header_y_max` (`double`)
  - `expected_header_text` (`std::string`)
  - `unit_symbol` (`std::string`)
- **`datasim::pdf::HeaderConditionBinding`**:
  - `header_x_span` (`HorizontalSpan`)
  - `header_y_min`, `header_y_max` (`double`)
  - `expected_header_text` (`std::string`)
  - `parameter_name` (`std::string`)
  - `unit_symbol` (`std::string`)
- **`datasim::pdf::ElectricalTableLayout`**:
  - Added optional fields `std::vector<HeaderUnitBinding> header_unit_bindings = {}` and `std::optional<HeaderConditionBinding> header_condition_binding = std::nullopt`.
  - Default-constructed `ElectricalTableLayout` instances leave both fields empty, preserving 100% backward compatibility with Phase 4C–4E callers.

### 2.2 Request Validation (`validate_request`)

In [`src/datasim/pdf/table_extraction.cpp`](../src/datasim/pdf/table_extraction.cpp):

1. Whenever `header_unit_bindings` is non-empty or `header_condition_binding` is present, `table.header_y_min < table.header_y_max` is required and must lie within `[table.region_y_min, table.region_y_max]`.
2. Each `HeaderUnitBinding` must target a distinct `minimum`, `typical`, or `maximum` value column, have a valid 2D box `[header_x_span.min, header_x_span.max] x [header_y_min, header_y_max]` inside the table region and header band, horizontally overlap its target value column span, and declare a single-token `unit_symbol` that appears in `expected_header_text` (either as the entire header or as a whitespace-delimited `(<unit_symbol>)` or `[<unit_symbol>]` token).
3. `HeaderConditionBinding` requires `table.condition_column.has_value()`, a valid 2D box inside the table region and header band that horizontally overlaps `*table.condition_column`, a single-token `parameter_name` present as a whitespace-delimited token in `expected_header_text`, and a single-token `unit_symbol` present in `expected_header_text`.

### 2.3 Source-Text & Geometric Verification (`verify_header_box_text`)

Before data-row segmentation:

1. Words on the selected PDF page overlapping `[header_x_span.min, header_x_span.max] x [header_y_min, header_y_max]` are collected.
2. Any word that partially straddles the configured header box boundary (`x0 < min || x1 > max || y0 < y_min || y1 > y_max`) causes extraction to fail closed (`"header word straddles configured header binding box: ..."`).
3. An empty header box fails closed (`"configured header binding box contains no selectable text"`).
4. Words inside the box are grouped into horizontal lines using `table.row_y_tolerance`, sorted left-to-right per line, joined with spaces, and compared for exact string equality against `expected_header_text`. Any mismatch fails closed (`"header binding text mismatch: expected '...' but found '...'"`).

### 2.4 Row-Level Resolution and Provenance Attachment

During row and value-column processing in `extract_electrical_table()`:

1. **Header Unit Resolution**:
   - If the row's `unit_column` cell is non-empty:
     - If a `HeaderUnitBinding` is configured for the active `qualifier` and `unit != binding.unit_symbol`, extraction fails closed (`"row <N> explicit unit '<unit>' conflicts with bound header unit '<binding.unit_symbol>'"`).
     - Otherwise (`unit == binding.unit_symbol` or no binding), the explicit row unit takes precedence and no header-unit evidence reference is attached.
   - If the row's `unit_column` cell is blank:
     - If a verified `HeaderUnitBinding` exists for the active `qualifier`, `observation.original_unit_symbol` is set to `binding.unit_symbol` verbatim (preserving Unicode symbols such as `"μF"` or `"µF"` without conversion), `observation.parsed_numeric_value` is parsed via `intake::parse_unambiguous_scalar(value)`, and a distinct `EvidenceLocation` (`id = "ev.<doc>.<table>.p<page>.header_unit.<min|typ|max>"`, `cell_or_region = "header_unit;column=<min|typ|max>"`) is registered in `manifest.evidence` and appended to `observation.evidence_refs`.
     - If no `HeaderUnitBinding` exists for the active `qualifier`, behavior is unchanged from Phase 4C (`original_unit_symbol = std::nullopt`, `parsed_numeric_value = std::nullopt`, and the blank-unit diagnostic is emitted).
2. **Header Condition Resolution**:
   - `entry.raw_conditions_text` always preserves the verbatim condition cell text when non-empty.
   - If `condition_text` contains `'='`, inline `PARAMETER=value UNIT` parsing is used; if `header_condition_binding` is also present, any conflict in parameter name or unit fails closed.
   - If `condition_text` does not contain `'='` and `header_condition_binding` is present:
     - If `intake::parse_unambiguous_scalar(condition_text)` succeeds, `observation.operating_conditions` is populated with `{binding.parameter_name, condition_text, binding.unit_symbol, cond_scalar, std::nullopt}`, and a distinct `EvidenceLocation` (`id = "ev.<doc>.<table>.p<page>.header_condition"`, `cell_or_region = "header_condition;column=condition"`) is registered in `manifest.evidence` and appended to `observation.evidence_refs`.
     - If `condition_text` is non-numeric (such as case code `"A"` in Vishay `150D`) or ambiguous, `observation.operating_conditions` remains empty, `entry.raw_conditions_text` and `entry.ambiguity_notes` are retained, and `"row <N>: operating condition left unresolved: ..."` is emitted.

---

## 3. CLI Adapter and Offline Evaluator Updates

- [`apps/datasheet_eval_dump/main.cpp`](../apps/datasheet_eval_dump/main.cpp) and [`apps/datasheet_extract/main.cpp`](../apps/datasheet_extract/main.cpp):
  - Added Windows UTF-8 command-line decoding via `CommandLineToArgvW(GetCommandLineW())` + `WideCharToMultiByte(CP_UTF8, ...)` so Unicode symbols (`"μF"`, `"µF"`, `"°C"`) are preserved losslessly regardless of the Windows active code page.
  - Added `--header-unit-binding <qualifier>|<xmin,ymin,xmax,ymax>|<unit_symbol>|<expected_header_text>` (repeatable) and `--header-condition-binding <xmin,ymin,xmax,ymax>|<parameter_name>|<unit_symbol>|<expected_header_text>`.
- [`scripts/eval/evaluate_pdf_corpus.py`](../scripts/eval/evaluate_pdf_corpus.py):
  - Forwards optional `header_unit_bindings` and `header_condition_binding` from `case["table_geometry"]` to `datasheet_eval_dump`.
  - Uses explicit `encoding="utf-8"` when capturing `datasheet_eval_dump` stdout/stderr.
  - Validates optional `expected_header_evidence_regions` on ground-truth observations against the resolved `evidence_refs` of extracted candidates.
