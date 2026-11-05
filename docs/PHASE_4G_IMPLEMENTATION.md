# Datasim — Phase 4G Implementation: Deterministic Inline Value/Unit Parsing

## 1. Overview

Phase 4G implements the bounded, source-verified inline value/unit parsing extension (**P2** from the Phase 4E evaluation roadmap) proposed in [ADR-0008](decisions/ADR-0008-inline-value-unit-parsing.md) and planned in [`docs/PHASE_4G_PLAN.md`](PHASE_4G_PLAN.md).

All production library changes are isolated strictly inside `datasim_pdf`:

- [`src/datasim/pdf/table_extraction.hpp`](../src/datasim/pdf/table_extraction.hpp)
- [`src/datasim/pdf/table_extraction.cpp`](../src/datasim/pdf/table_extraction.cpp)

Zero changes were made to `datasim`, `datasim_component`, `datasim_vertical`, or `datasim_intake`.

---

## 2. Existing Pre-Phase-4G Behavior

Prior to Phase 4G, `datasim_pdf::extract_electrical_table()` processed each configured value column (`minimum_column`, `typical_column`, `maximum_column`) under the assumption that the entire value cell text was a bare numeric token (optionally followed by trailing `*`/`†` footnote markers) while the unit symbol was supplied either by:

1. A separately configured `unit_column` on the same row (Phase 4C), or
2. A source-verified `HeaderUnitBinding` for that value column when the row's `unit_column` cell was blank (Phase 4F).

When a PDF table cell placed both the numeric token and unit symbol inside a single visual value cell (such as `"200 V"`, `"75 V"`, `"0.4 W"`, `"220 pF"`, `"10 nF"`, `"1.0 µF"`):

- Passing `"200 V"` directly to `datasim::intake::parse_unambiguous_scalar("200 V")` rejected the token as `contains_unit_or_text`, leaving `parsed_numeric_value == std::nullopt`.
- Attempting to split a single visual column into separate `typical_column` and `unit_column` spans required sub-point `x` coordinates (`0.001 pt` in `vishay_crcw_e3.pdf`, `0.43 pt` in `vishay_vj_commercial_mlcc.pdf`) or failed closed entirely when right-aligned numbers and trailing units in different rows overlapped horizontally (`vishay_mrs16_mrs25.pdf`, where `"200"` ends at `xMax = 304.742` while `"W"` in `"0.4 W"` starts at `xMin = 304.499`), or when a table contained multiple inline value+unit columns (`MINIMUM` and `MAXIMUM` in `vishay_vj_commercial_mlcc.pdf`) with only a single `unit_column` field available in `ElectricalTableLayout`.

---

## 3. Production Contracts in `datasim_pdf`

### 3.1 `InlineValueUnitBinding` Configuration

Added to [`src/datasim/pdf/table_extraction.hpp`](../src/datasim/pdf/table_extraction.hpp):

- **`datasim::pdf::InlineValueUnitBinding`**:
  - `qualifier` (`component::ValueQualifier::minimum`, `typical`, or `maximum`)
  - `allowed_unit_symbols` (`std::vector<std::string>`) — non-empty list of explicit single-token unit symbols permitted as inline trailing unit tokens in that column (e.g., `{"V", "W"}` or `{"pF", "nF"}`).
- **`datasim::pdf::ElectricalTableLayout`**:
  - Added optional field `std::vector<InlineValueUnitBinding> inline_value_unit_bindings = {}`.
  - Default-constructed `ElectricalTableLayout` instances leave `inline_value_unit_bindings` empty, preserving 100% backward compatibility with all Phase 4C–4F callers.

### 3.2 `ValueCellProvenance` Sidecar on `ExtractionResult`

To preserve the complete combined source cell text (`"200 V"`) alongside the established semantics of `ParameterObservation::original_value_text` (`"200"`) and `ParameterObservation::original_unit_symbol` (`"V"`) without modifying `datasim_component` or `datasim_intake` contracts or abusing diagnostic/ambiguity fields, Phase 4G adds `ValueCellProvenance` to `datasim::pdf::ExtractionResult`:

- **`datasim::pdf::ValueCellProvenance`**:
  - `observation_id` (`std::string`)
  - `value_evidence_id` (`std::string`)
  - `row_index` (`std::size_t`)
  - `qualifier` (`component::ValueQualifier`)
  - `raw_cell_text` (`std::string`) — exact combined source cell text inside the configured value column (e.g., `"200 V"`, `"200* V"`, `"1,000 V"`).
  - `raw_value_token` (`std::string`) — extracted numeric value token (or full stripped cell text when unsplit).
  - `raw_inline_unit_token` (`std::optional<std::string>`) — extracted inline unit token when split, or `std::nullopt` when no inline unit token was split.
- **`datasim::pdf::ExtractionResult`**:
  - Added `std::vector<ValueCellProvenance> value_cell_provenance = {}` populated in 1-to-1 correspondence with `candidate_bundle->manifest.observations`.

---

## 4. Request Validation, Geometry, and Token Parsing

### 4.1 Request Validation (`validate_request`)

In [`src/datasim/pdf/table_extraction.cpp`](../src/datasim/pdf/table_extraction.cpp):

1. Each `InlineValueUnitBinding` must target `ValueQualifier::minimum`, `typical`, or `maximum`, and the corresponding value column span (`minimum_column`, `typical_column`, or `maximum_column`) must be configured on `table`.
2. Duplicate `InlineValueUnitBinding` entries for the same `ValueQualifier` fail closed (`"duplicate inline_value_unit_bindings entry for the same value column"`).
3. `allowed_unit_symbols` must be non-empty; every symbol must be a non-empty single-token string containing no ASCII whitespace and no `*` or `†` footnote markers, with no duplicate symbols in the same binding.

### 4.2 PDF Word Bounding-Box Geometry Policy (`row_value_cell`)

Phase 4G preserves all Phase 4C–4F geometric safety invariants and adds an intra-cell horizontal word collision check:

1. **Zero Cross-Column Overlap**: Configured column spans (`parameter_column`, `condition_column`, `minimum_column`, `typical_column`, `maximum_column`, `unit_column`) must remain strictly non-overlapping. Any word straddling a column boundary or falling in an unassigned horizontal gap fails closed.
2. **Intra-Cell Horizontal Word Non-Overlap**: Within a value column cell on a data row, words are sorted by `x0` (`xMin`). If any two adjacent words in the cell have horizontally overlapping Poppler bounding boxes (`words[i - 1].x1 > words[i].x0`), extraction fails closed (`"row <N> overlapping word bounding boxes in value cell"`).
3. **No Heuristic Overlap Tolerance**: Zero horizontal or vertical overlap slack was introduced. Resolving Phase 4E P2 by configuring a single value column span covering both the numeric token and trailing inline unit eliminates the artificial sub-point value/unit boundary splits while keeping every column boundary strict.

### 4.3 Deterministic Token Parsing & Footnote Rules (`parse_inline_value_unit_cell`)

When a value column has an active `InlineValueUnitBinding`:

1. `raw_cell_text` is captured verbatim from the words inside the configured value column cell.
2. Whitespace-delimited tokens in `raw_cell_text` are inspected:
   - **More than 2 tokens** (e.g., `"200 V AC"`): Fails closed (`"row <N> inline value/unit cell '<cell>' has unsupported multi-token format"`).
   - **Exactly 2 tokens (`<token0> <token1>`)**:
     - Trailing `*` and `†` footnote markers are stripped from `<token0>` (yielding `value_part`, `val_fn`) and from `<token1>` (yielding `unit_part`, `unit_fn`).
     - Both `value_part` and `unit_part` must be non-empty.
     - If `unit_part` is **not** in `binding.allowed_unit_symbols`, extraction **fails closed** (`"row <N> inline unit token '<unit_part>' in cell '<cell>' is not in allowed_unit_symbols"`).
     - If `unit_part` is in `binding.allowed_unit_symbols`:
       - `raw_value_token = value_part`
       - `raw_inline_unit_token = unit_part`
       - Footnote references from either `val_fn` (`"200* V"`) or `unit_fn` (`"200 V*"`) are recorded in `observation.footnote_refs` and `entry.ambiguity_notes = "footnote marker requires engineering review"`.
       - `raw_value_token` is evaluated by the unchanged `intake::parse_unambiguous_scalar(raw_value_token)`. If `raw_value_token` is unambiguous and no footnote marker is present, `parsed_numeric_value` is populated; if `raw_value_token` is locale-ambiguous (`"1,000"`), a range (`"0.22..0.47"`), a tolerance (`"±5"`), or carries a footnote marker, `parsed_numeric_value` remains `std::nullopt` with diagnostic and ambiguity notes preserved.
   - **Exactly 1 token (`<token0>`)**:
     - Trailing `*`/`†` footnote markers are stripped.
     - If the stripped token is a bare scalar or numeric expression (such as `"55"`, `"1,000"`, `"0.22..0.47"`), `raw_inline_unit_token` is `std::nullopt` and unit resolution falls back to the row `unit_column` or `HeaderUnitBinding` (if any); if no unit source exists, `original_unit_symbol` and `parsed_numeric_value` remain `std::nullopt` with `"row <N>: blank unit left unresolved"`.
     - If the single token is non-numeric text (e.g., glued `"200V"` without whitespace or `"N/A"`), extraction fails closed (`"row <N> inline value/unit cell '<cell>' has unseparated or non-numeric token"`).

### 4.4 Deterministic Unit Precedence and Conflict Resolution

For each value cell, `datasim_pdf` resolves up to three explicit unit sources (`inline_unit`, `row_unit` from `unit_column`, and `header_unit` from Phase 4F `HeaderUnitBinding`):

| Inline Unit | Row `unit_column` | Header `HeaderUnitBinding` | Outcome |
| --- | --- | --- | --- |
| Present (`"V"`) | Blank | Absent | `original_unit_symbol = "V"`; `evidence_refs = {param, value}` |
| Present (`"V"`) | Blank | Present (`"V"`) | Consistent; inline unit takes precedence (`evidence_refs = {param, value}`) |
| Present (`"V"`) | Blank | Present (`"mV"`) | **Fails closed** (`inline unit 'V' conflicts with bound header unit 'mV'`) |
| Present (`"V"`) | Present (`"V"`) | Absent or `"V"` | Consistent; inline/row unit `"V"` used (`evidence_refs = {param, value}`) |
| Present (`"V"`) | Present (`"mV"`) | Any | **Fails closed** (`inline unit 'V' conflicts with explicit row unit 'mV'`) |
| Absent | Present (`"V"`) | Absent or `"V"` | Row unit `"V"` used (Phase 4C/4F behavior) |
| Absent | Present (`"V"`) | Present (`"mV"`) | **Fails closed** (Phase 4F row/header conflict) |
| Absent | Blank | Present (`"V"`) | Header unit `"V"` used; `header_unit` evidence ref appended (Phase 4F behavior) |
| Absent | Blank | Absent | Missing unit: `original_unit_symbol = std::nullopt`, `parsed_numeric_value = std::nullopt`, diagnostic emitted |

---

## 5. CLI Adapter and Offline Evaluator Updates

- [`apps/datasheet_extract/main.cpp`](../apps/datasheet_extract/main.cpp) and [`apps/datasheet_eval_dump/main.cpp`](../apps/datasheet_eval_dump/main.cpp):
  - Added repeatable `--inline-unit-binding <qualifier>|<unit1,unit2,...>` CLI flag.
  - Serialized `raw_cell_text`, `raw_value_token`, and `raw_inline_unit_token` on each dumped observation JSON object as well as a top-level `value_cell_provenance` array.
- [`scripts/eval/evaluate_pdf_corpus.py`](../scripts/eval/evaluate_pdf_corpus.py):
  - Forwards optional `inline_value_unit_bindings` from `case["table_geometry"]` to `datasheet_eval_dump`.
  - Validates optional `expected_raw_cell_text` on ground-truth observations as part of `raw_value` agreement whenever provided in the ground-truth annotation file.
  - Classifies inline value/unit fail-closed diagnostics (`inline unit`, `inline value/unit`, `overlapping word bounding boxes`) in the failure taxonomy.

