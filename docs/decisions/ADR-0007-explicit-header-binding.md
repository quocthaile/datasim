# ADR-0007: Explicit Column-Header Unit and Condition Binding in `datasim_pdf`

Date: 2026-11-05

Status: **IMPLEMENTED on `feat/phase-4f-explicit-header-binding` (local verification and Phase 4E pilot re-evaluation completed; awaiting hosted Windows CI on the Phase 4F pull request).**

## Context

Phase 4E ([validation report](../PHASE_4E_VALIDATION.md), merged into `main` via PR #9 at `b45c32224e0316dd9f93b6e9bfc3ab89cd3ceaf4`) benchmarked the Phase 4C `datasim_pdf::extract_electrical_table()` adapter against a human-annotated pilot corpus of real resistor and capacitor datasheets.

Across the 15 matched observations in the 4 extractable real-datasheet sub-bands (`TP = 9`, `FP = 6`, `FN = 6`, `Precision = 0.6000`, `Recall = 0.6000`, `F1 = 0.6000`), Phase 4E identified two recurring, bounded header-metadata patterns:

1. **Header-Declared Value Units (`REAL-CASE-07-150D-RATINGS-SUBBAND`, `5/5` rows)**:
   In the Vishay Sprague `150D` tantalum capacitor `STANDARD RATINGS` table, the unit symbol `(μF)` is declared in the column header (`CAPACITANCE AT +25 °C 120 Hz (μF)`) rather than repeated in a dedicated `Unit` column on each data row. Because Phase 4C ignores words inside `[header_y_min, header_y_max]` and requires a non-empty per-row `unit_column` cell to populate `original_unit_symbol` and `parsed_numeric_value`, all 5 extracted rows emitted `original_unit_symbol = std::nullopt` and `parsed_numeric_value = std::nullopt` (`5` `unit_mismatch` and `5` `ambiguous_numeric` findings).
2. **Header-Declared Operating Condition Quantities and Units (`REAL-CASE-05-VJ-MLCC-C0G-MIN-BAND`, `5/5` rows)**:
   In the Vishay Vitramon `VJ Commercial Series` MLCC `QUICK REFERENCE DATA` table, the condition quantity and unit are declared in the column header (`MAXIMUM VOLTAGE (V)`), while data-row cells contain bare numeric strings (`"100"`, `"250"`, `"500"`, `"630"`, `"630"`). Because Phase 4C requires inline `PARAMETER=value UNIT` syntax inside the condition cell, all 5 rows emitted unresolved-condition diagnostics while leaving `operating_conditions` empty.

Phase 4F requires a narrowly scoped, deterministic mechanism inside `datasim_pdf` to bind caller-configured, source-verified column-header units and condition metadata without introducing heuristic header recognition, unit inference, automatic unit conversion, or changes to `datasim_intake`, `datasim_component`, `datasim_vertical`, or `datasim`.

## Decision

1. **Keep the extension strictly inside `datasim_pdf`**:
   Add two optional configuration structs to `datasim::pdf::ElectricalTableLayout` in [`src/datasim/pdf/table_extraction.hpp`](../../src/datasim/pdf/table_extraction.hpp):
   - `HeaderUnitBinding`:
     - `component::ValueQualifier qualifier` (`minimum`, `typical`, or `maximum`)
     - `HorizontalSpan header_x_span` and `double header_y_min`, `double header_y_max`
     - `std::string expected_header_text`
     - `std::string unit_symbol`
   - `HeaderConditionBinding`:
     - `HorizontalSpan header_x_span` and `double header_y_min`, `double header_y_max`
     - `std::string expected_header_text`
     - `std::string parameter_name`
     - `std::string unit_symbol`
   - `ElectricalTableLayout` gains `std::vector<HeaderUnitBinding> header_unit_bindings` and `std::optional<HeaderConditionBinding> header_condition_binding`. Default-constructed layouts leave both empty, preserving 100% backward compatibility with all Phase 4C–4E requests.

2. **Enforce Fail-Closed Geometric and Source-Text Verification**:
   - Every configured header box `[header_x_span.min, header_x_span.max] x [header_y_min, header_y_max]` must have `min < max` on both axes, lie within `[table.region_x_min, table.region_x_max]` and `[table.header_y_min, table.header_y_max]` (requiring `table.header_y_min < table.header_y_max`), and horizontally overlap the target column (`minimum_column`, `typical_column`, `maximum_column`, or `condition_column`).
   - Duplicate `HeaderUnitBinding` entries for the same `ValueQualifier`, or a `HeaderConditionBinding` when `table.condition_column` is absent, fail closed.
   - Words in the selected PDF page that intersect a configured header box must lie completely inside that box (any word straddling the boundary fails closed). The words inside the header box are grouped into horizontal lines using `table.row_y_tolerance` and joined in deterministic top-to-bottom, left-to-right order. The resulting text must match `expected_header_text` exactly; missing or mismatched header text fails closed.
   - `unit_symbol` must be a non-empty single-token string and must appear verbatim in `expected_header_text` as the entire header text, a parenthesized token `"(" + unit_symbol + ")"`, or a bracketed token `"[" + unit_symbol + "]"`. No unit is ever inferred from parameter names, part families, or electrical conventions, and no unit conversion (e.g., `μF` to `uF` or `F`) is performed.
   - For `HeaderConditionBinding`, `parameter_name` must also be a non-empty single-token string that appears verbatim as a whitespace-delimited token in `expected_header_text`.

3. **Precedence, Conflict Detection, and Separate Header Provenance**:
   - **Header Unit Binding**:
     - Validated header unit bindings create a distinct `EvidenceLocation` (`id = "ev.<doc>.<table>.p<page>.header_unit.<min|typ|max>"`, `cell_or_region = "header_unit;column=<min|typ|max>"`, `row_label = expected_header_text`, `column_label = "<min|typ|max>_header"`).
     - When a data row's `unit_column` cell is blank (`unit.empty()`) and the target value column has a verified `HeaderUnitBinding`, the observation receives `original_unit_symbol = binding.unit_symbol`, parses `parsed_numeric_value = intake::parse_unambiguous_scalar(value)`, and appends the header unit `EvidenceLocation::id` to `observation.evidence_refs` (`{parameter_evidence.id, evidence.id, header_unit_evidence.id}`).
     - When a data row has an explicit `unit_column` cell (`!unit.empty()`) and the target column also has a `HeaderUnitBinding`:
       - If `unit == binding.unit_symbol`, the row's explicit unit takes precedence (`evidence_refs = {parameter_evidence.id, evidence.id}`).
       - If `unit != binding.unit_symbol`, extraction **fails closed** rather than silently resolving the row/header unit conflict.
   - **Header Condition Binding**:
     - Validated header condition bindings create a distinct `EvidenceLocation` (`id = "ev.<doc>.<table>.p<page>.header_condition"`, `cell_or_region = "header_condition;column=condition"`, `row_label = expected_header_text`, `column_label = "condition_header"`).
     - Raw condition cell text is always preserved in `IntakeObservationEntry::raw_conditions_text`.
     - If a row's condition cell uses explicit `PARAMETER=value UNIT` syntax:
       - If no `HeaderConditionBinding` is configured, Phase 4C parsing applies unchanged.
       - If a `HeaderConditionBinding` is configured, any inline condition with a conflicting parameter name or unit fails closed; if consistent (`cond.parameter_name == binding.parameter_name && cond.unit_symbol == binding.unit_symbol`), the explicit inline condition takes precedence.
     - If a row's condition cell does not contain `=` and a `HeaderConditionBinding` is configured:
       - If `intake::parse_unambiguous_scalar(condition_text)` succeeds (e.g., `"100"`, `"250"`, `"500"`, `"630"`), `datasim_pdf` constructs `OperatingCondition{binding.parameter_name, condition_text, binding.unit_symbol, scalar, std::nullopt}` and appends `header_condition_evidence.id` to `observation.evidence_refs`.
       - If `parse_unambiguous_scalar(condition_text)` fails (e.g., package/case code `"A"`, locale-ambiguous `"1,000"`, or footnote `"100*"`), `operating_conditions` remains empty, `raw_conditions_text` and `ambiguity_notes` are retained, and the diagnostic `"row <N>: operating condition left unresolved: <text>"` is emitted.

## Consequences

- `datasim_intake`, `datasim_component`, `datasim_vertical`, and `datasim` contracts remain completely unchanged.
- Every extracted observation remains in `ReviewState::candidate` with complete row, value, and header provenance.
- Multi-value-column tables with distinct column-header units (e.g., `CAPACITANCE (μF)` in `typ` and `MAX. DCL (μA)` in `max`) can be bound independently per `ValueQualifier`.
- Arbitrary header prose, mismatched header coordinates, row/header unit conflicts, and inline/header condition conflicts fail closed with deterministic diagnostics.
- Non-numeric condition codes (such as `"A"` in `150D`) remain safely unresolved rather than being coerced into fake numeric conditions.
