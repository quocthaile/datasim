# ADR-0008: Deterministic Inline Value/Unit Cell Parsing in `datasim_pdf`

- **Status**: Accepted (Implemented on `feat/phase-4g-inline-value-unit-parsing`)
- **Date**: 2026-04-06
- **Phase**: Phase 4G

## Context

Phase 4E evaluated the deterministic PDF table extractor (`datasim_pdf`) against an 8-case pilot corpus of real passive component datasheets (`docs/PHASE_4E_VALIDATION.md`), and Phase 4F (`ADR-0007`) resolved failure pattern **P1** via explicit, source-verified column-header unit and condition bindings (`HeaderUnitBinding` and `HeaderConditionBinding`).

The second highest-leverage limitation identified in Phase 4E is **P2 — Inline Value + Unit Single-Cell Encoding**:

1. Several manufacturer electrical and capacitance tables place the numeric value and the unit symbol inside a single visual value column cell rather than in separate `value` and `unit` columns or in a column header:
   - **Vishay `MRS16 / MRS25` (`vishay_mrs16_mrs25.pdf`, Page 1)**: The `MRS16` value column (`x = [287.98, 315.12]`) encodes cells such as `0.4 W`, `200 V`, `300 V`, and `75 V`. Across rows, 3-digit numbers (`200` at `x = [291.368, 304.742]`, `300` at `x = [291.360, 304.736]`) extend further right than the left edge of the unit token on 2-digit or decimal rows (`W` in `0.4 W` at `x = [304.499, 311.889]`, `V` in `75 V` at `x = [304.683, 309.558]`). Any attempt to split the visual column into non-overlapping `maximum_column` and `unit_column` horizontal spans fails across rows because `xMax("200") = 304.742 > xMin("W") = 304.499`.
   - **Vishay `VJ Commercial Series MLCC` (`vishay_vj_commercial_mlcc.pdf`, Page 2)**: The `C0G (NP0)` selection chart encodes inline values and units in both the `MINIMUM` column (`1.0 pF`, `56 pF`, `100 pF`) and the `MAXIMUM` column (`220 pF`, `820 pF`, `4.7 nF`, `10 nF`, `12 nF`). Not only do 2-digit and 3-digit rows collide horizontally if split into separate value/unit columns, the `MAXIMUM` column mixes `pF` and `nF` across rows of the same column, so a single Phase 4F `HeaderUnitBinding` cannot represent the column's units without implicit unit conversion.
2. In Phase 4C–4F, `extract_electrical_table()` joins all words inside a value column (`minimum_column`, `typical_column`, `maximum_column`) into a single string (`raw_value_text = "200 V"`) and passes that full string directly to `intake::parse_unambiguous_scalar("200 V")`. Because `parse_unambiguous_scalar()` enforces a strict numeric-only grammar, `"200 V"` fails scalar parsing (`parsed_numeric_value = std::nullopt`) and emits an `"ambiguous numeric text '200 V'"` diagnostic, while leaving `original_unit_symbol` blank unless a separate `unit_column` or `HeaderUnitBinding` was configured.
3. Furthermore, `datasim_intake::validate_intake_manifest()` enforces that whenever `ParameterObservation::parsed_numeric_value` is populated, `parse_unambiguous_scalar(*observation.original_value_text)` must succeed and match `parsed_numeric_value` within `1e-12` (`src/datasim/intake/validation/intake_validation.cpp`). Therefore, `ParameterObservation::original_value_text` must hold the exact source numeric token (`"200"`), `ParameterObservation::original_unit_symbol` must hold the exact source unit token (`"V"`), and the complete source cell text (`"200 V"`) must be preserved in an explicit provenance contract rather than stuffed into `diagnostics` or `ambiguity_notes`.

Phase 4G requires a bounded, opt-in extension in `datasim_pdf` that parses explicitly configured inline value/unit cells, preserves full cell provenance, enforces deterministic unit precedence and fail-closed conflict rules, and leaves `datasim`, `datasim_component`, `datasim_vertical`, and `datasim_intake` completely unchanged.

## Decision

### 1. Explicit Opt-In Configuration (`InlineValueUnitBinding`)

Add `InlineValueUnitBinding` to `datasim::pdf` (`src/datasim/pdf/table_extraction.hpp`) and an optional `inline_value_unit_bindings` vector to `ElectricalTableLayout`:

```cpp
struct InlineValueUnitBinding {
    component::ValueQualifier qualifier = component::ValueQualifier::typical;
    std::vector<std::string> allowed_unit_symbols;
};
```

In `ElectricalTableLayout`:

```cpp
std::vector<InlineValueUnitBinding> inline_value_unit_bindings;
```

- **Backward compatibility**: When `inline_value_unit_bindings` is empty (the default) or has no entry for a given `ValueQualifier`, `extract_electrical_table()` executes the exact Phase 4C–4F code path with zero behavior change.
- **No automatic unit inference or conversion**: Every inline unit symbol accepted for a value column (`minimum`, `typical`, or `maximum`) must be explicitly listed in `allowed_unit_symbols` for that `qualifier`. No unit is ever inferred from parameter names, electrical conventions, or adjacent rows, and no scaling or unit conversion (e.g., `nF` to `pF` or `mV` to `V`) is performed.
- **Request validation (`validate_request`)**:
  - `qualifier` must be one of `ValueQualifier::minimum`, `ValueQualifier::typical`, or `ValueQualifier::maximum`.
  - At most one `InlineValueUnitBinding` may be configured per `ValueQualifier`.
  - `allowed_unit_symbols` must be non-empty, contain no duplicate strings, and every symbol must satisfy `is_valid_inline_unit_token()`:
    - Non-empty single token with no whitespace or ASCII control characters.
    - Does not start with an ASCII digit (`0-9`), sign (`+`, `-`), decimal point (`.`), or footnote marker (`*`).
    - Does not contain footnote/punctuation/delimiter characters (`*`, `,`, `;`, `=`, `(`, `)`, `[`, `]`).
    - Contains at least one unit character (ASCII letter `A-Z`/`a-z`, `%`, or non-ASCII UTF-8 symbol byte such as `Ω`, `μ`, `µ`, `°`).

### 2. Source-Cell Provenance Contract (`ValueCellProvenance`)

To preserve both the established `ParameterObservation` field semantics (`original_value_text = "200"`, `original_unit_symbol = "V"`, `parsed_numeric_value = 200.0`) and the complete verbatim cell text (`"200 V"`) without modifying `datasim_component` or `datasim_intake`, add `ValueCellProvenance` to `datasim::pdf` and `value_cell_provenance` to `ExtractionResult`:

```cpp
struct ValueCellProvenance {
    component::ObservationId observation_id;
    component::EvidenceId value_evidence_id;
    std::size_t row_index = 0;
    component::ValueQualifier qualifier = component::ValueQualifier::unspecified;
    std::string raw_cell_text;
    std::string raw_value_token;
    std::optional<std::string> raw_inline_unit_token;

    auto operator<=>(const ValueCellProvenance&) const = default;
};
```

In `ExtractionResult`:

```cpp
std::vector<ValueCellProvenance> value_cell_provenance;
```

For every emitted candidate observation:

1. `ValueCellProvenance::raw_cell_text` records the complete, whitespace-joined text of the source value cell (`"200 V"`, `"1,000 V"`, `"200 V*"`, `"1.0 μF"`, or `"200"` when unsplit).
2. `ValueCellProvenance::raw_value_token` records the exact numeric token assigned to `ParameterObservation::original_value_text` (`"200"`, `"1,000"`, `"200*"`).
3. `ValueCellProvenance::raw_inline_unit_token` records the inline unit token extracted from the value cell (`"V"`, `"pF"`, `"nF"`, `"μF"`), or `std::nullopt` when no inline unit token was present in the cell.
4. `observation_id` and `value_evidence_id` link the provenance entry 1-to-1 to the corresponding `ParameterObservation` and value-cell `EvidenceLocation`.

### 3. Bounded Inline Token Parsing Grammar

When an `InlineValueUnitBinding` is active for a value column (`qualifier`), a non-empty, non-placeholder (`"-"`, `"—"`) cell with text `raw_cell_text` is tokenized on whitespace into `tokens`:

- **`tokens.size() > 2`**: Fails closed (`std::runtime_error` with `"row <N> inline value cell '<raw_cell_text>' contains unsupported multi-token representation"`). Multi-word phrases, three-token ranges (`"10 to 20 V"`), or embedded condition fragments inside a value cell are rejected.
- **`tokens.size() == 2` (`val_token = tokens[0]`, `unit_cand = tokens[1]`)**:
  1. **Unit token verification**:
     - If `unit_cand` exactly matches an entry in `binding.allowed_unit_symbols`: `inline_unit = unit_cand`, `unit_has_footnote = false`.
     - Else if `unit_cand.size() > 1 && unit_cand.back() == '*'` and `unit_cand.substr(0, unit_cand.size() - 1)` exactly matches an entry in `binding.allowed_unit_symbols` (e.g., `"200 V*"`): `inline_unit = unit_cand.substr(0, unit_cand.size() - 1)`, `unit_has_footnote = true`.
     - Otherwise: Fails closed (`"row <N> inline unit token '<unit_cand>' is not permitted by configured inline value/unit binding"`).
  2. **Value token verification**:
     - `val_token` must satisfy one of three bounded numeric forms:
       a. **Strict scalar**: `intake::parse_unambiguous_scalar(val_token).has_value()`.
       b. **Footnote-marked scalar** (when `!unit_has_footnote`): `val_token.size() > 1 && val_token.back() == '*'` and `intake::parse_unambiguous_scalar(val_token.substr(0, val_token.size() - 1)).has_value()` (e.g., `"200* V"`).
       c. **Locale-ambiguous comma number** (when `!unit_has_footnote`): `is_locale_ambiguous_number(val_token)` — optional leading `-`, ASCII digits separated by `,` with at least one comma between digits (e.g., `"1,000 V"`). Preserved as `original_value_text = "1,000"`, `original_unit_symbol = "V"`, `parsed_numeric_value = std::nullopt`, with diagnostic `"row <N>: ambiguous numeric text '1,000' was preserved without a parsed scalar"`.
     - Any other `val_token` — including ranges (`"0.22..0.47 uF"`, `"1-5 V"`), tolerances (`"±5 %"`, `"+/-10 %"`), inequalities (`"<0.1 A"`), or non-numeric words — **fails closed** (`"row <N> inline value cell '<raw_cell_text>' contains unsupported value representation '<val_token>'"`).
- **`tokens.size() == 1` (`val_token = tokens[0]`, `inline_unit = std::nullopt`)**:
  - `val_token` must satisfy the same three bounded numeric forms (strict scalar, footnote-marked scalar, or locale-ambiguous comma number).
  - Undelimited value+unit tokens (`"200V"`, `"10nF"`), single-token ranges (`"0.22..0.47"`), or single-token tolerances (`"±5%"`) **fail closed** (`"row <N> inline value cell '<raw_cell_text>' contains unsupported value representation '<val_token>'"`).

### 4. Deterministic Unit Precedence and Conflict Matrix

For each observation in row `N` and column `qualifier`, three unit sources may exist:
- `inline_unit`: Extracted from the second token of the value cell when `InlineValueUnitBinding` is active for `qualifier`.
- `row_unit`: Extracted from `layout.unit_column` on row `N`.
- `header_unit`: Configured via Phase 4F `HeaderUnitBinding` for `qualifier`.

| `inline_unit` | `row_unit` | `header_unit` | Deterministic Resolution |
|---|---|---|---|
| Present (`"V"`) | Empty | Absent | **Inline unit used** (`original_unit_symbol = "V"`). Value cell `EvidenceLocation` is the sole unit/value evidence. |
| Absent | Empty | Present (`"V"`) | **Header unit used** (`original_unit_symbol = "V"`). If `InlineValueUnitBinding` is active for `qualifier` and `"V"` is not in `allowed_unit_symbols`, **fails closed**; otherwise attaches `header_unit` `EvidenceLocation` as in Phase 4F. |
| Present (`"V"`) | Empty | Present (`"V"`) | **Consistent inline + header unit**: Allowed. Cell-level `inline_unit` (`"V"`) takes precedence; no redundant header evidence ref is required. |
| Present (`"V"`) | Empty | Present (`"mV"`) | **Conflicting inline vs. header unit**: **Fails closed** (`"row <N> inline unit 'V' conflicts with bound header unit 'mV'"`). Never converts implicitly. |
| Present (`"V"`) | Present (`"V"`) | Absent or `"V"` | **Consistent inline + row unit**: Allowed (`original_unit_symbol = "V"`). |
| Present (`"V"`) | Present (`"mV"`) | Any | **Conflicting inline vs. row unit**: **Fails closed** (`"row <N> inline unit 'V' conflicts with explicit row unit 'mV'"`). |
| Absent | Present (`"V"`) | Absent or `"V"` | **Row unit used** (`original_unit_symbol = "V"`). If `InlineValueUnitBinding` is active for `qualifier` and `"V"` is not in `allowed_unit_symbols`, **fails closed**. |
| Absent | Empty | Absent | **Missing unit**: Emits candidate observation with `original_unit_symbol = std::nullopt`, `parsed_numeric_value = std::nullopt`, `ambiguity_notes = "Source unit cell was blank; numeric value was not asserted."`, and diagnostic `"row <N>: unit cell is blank; numeric value was not asserted"`. |
| Unpermitted token (`"X"` or `"kV"` not in `allowed_unit_symbols`) | Any | Any | **Ambiguous / unpermitted unit token**: **Fails closed** (`"row <N> inline unit token '<token>' is not permitted by configured inline value/unit binding"`). |

### 5. Footnote Handling on Inline Value/Unit Cells

Datasheets frequently attach a footnote marker (`*`) either to the numeric token (`"200* V"`) or to the trailing unit token (`"200 V*"`).

- In both cases, `evidence.footnote_refs` is populated with `{"*"}` and `ValueCellProvenance::raw_cell_text` preserves the exact asterisk-marked cell string (`"200* V"` or `"200 V*"`).
- `ParameterObservation::original_unit_symbol` receives the clean verified unit symbol (`"V"`), never `"V*"`.
- `ParameterObservation::parsed_numeric_value` is forced to `std::nullopt` so no footnote-qualified value is ever asserted as an unconditional scalar.
- When the asterisk appeared on the unit token (`"200 V*"`), `original_value_text` is `"200"`, `entry.ambiguity_notes` is set to `"Footnote-qualified inline value cell preserved without asserted numeric scalar."`, and a diagnostic `"row <N>: footnote-qualified inline value cell '200 V*'; numeric value was not asserted"` is recorded.

### 6. PDF Geometry, Word Ownership, and Intra-Cell Bounding-Box Policy

1. **Zero weakening of column-level geometry**:
   - `validate_request()` continues to enforce that all configured `HorizontalSpan` columns (`parameter_column`, `symbol_column`, `condition_column`, `minimum_column`, `typical_column`, `maximum_column`, `unit_column`) are pairwise non-overlapping after sorting by `min`.
   - Every word inside the table vertical span must have its horizontal center `center = (word.x0 + word.x1) / 2.0` inside exactly one column span AND have both edges wholly inside that span (`word.x0 >= span.min && word.x1 <= span.max`). Any word straddling a column boundary (`matches != 1`) fails closed immediately.
   - Instead of introducing an overlap tolerance between separate `value` and `unit` columns, Phase 4G configures the single visual value column span (`[span.min, span.max]`) to enclose both the numeric word and the trailing unit word, while keeping `layout.unit_column` placed over an unused non-overlapping blank gutter (or an optional secondary unit column).
2. **Deterministic intra-cell word ordering and horizontal collision check**:
   - Within each row and column cell, Poppler `<word>` boxes are sorted deterministically by `(x0, x1, text)`.
   - In Poppler `pdftotext -bbox-layout` output on real and synthetic PDFs, a space-delimited inline value and unit (`200 V`, `0.4 W`, `220 pF`) are emitted as two distinct `<word>` elements separated by the font's space glyph width (typically `+2.0..+2.5 pt` at 9–10 pt font size; e.g., `+2.221 pt` in Vishay `MRS16`).
   - To prevent duplicate or overlapping cell assignments within a value cell (`minimum_column`, `typical_column`, `maximum_column`), `extract_electrical_table()` verifies that sorted words inside a value cell are strictly non-overlapping along the horizontal axis:
     ```cpp
     if (words[i + 1].x0 < words[i].x1) {
         throw std::runtime_error("row <N> value cell has overlapping or duplicate word bounding boxes");
     }
     ```
   - No positive overlap tolerance is required or permitted. Any duplicate text stream or colliding word boxes inside a value cell fails closed.
3. **Deterministic observation IDs and provenance**:
   - Observation IDs (`obs-<slug>-p<page>-r<row>-<qualifier>`) and evidence IDs (`ev-<slug>-p<page>-r<row>-<qualifier>`) remain deterministic and unchanged across runs.
   - The value cell `EvidenceLocation::bounding_box` spans the exact union of the numeric word and inline unit word bounding boxes (`min(x0), min(y0), max(x1), max(y1)`), and `EvidenceLocation::excerpt_redacted` is `<redacted-inline-value-unit-cell>` when an inline unit token is present (and `<redacted-value-cell>` otherwise).

## Consequences

### Positive

- **Directly resolves Phase 4E P2**: Single-cell value+unit encodings (`200 V`, `75 V`, `0.4 W`, `1.0 pF`, `56 pF`, `220 pF`, `820 pF`, `4.7 nF`, `10 nF`, `12 nF`, `1.0 μF`) are deterministically extracted with verified scalar values, exact unit symbols, and complete cell provenance.
- **Supports mixed unit prefixes within one column**: Because `InlineValueUnitBinding::allowed_unit_symbols` accepts an explicit allowlist of source-verified unit symbols (e.g., `{"pF", "nF"}`), columns that transition from `pF` to `nF` across rows (`VJ MLCC` `MAXIMUM` column) are handled cleanly without unit inference or conversion.
- **Preserves all upstream/downstream contracts**: `datasim`, `datasim_component`, `datasim_vertical`, and `datasim_intake` require zero source changes. All extracted observations remain in `ReviewState::candidate`.
- **Strict fail-closed safety**: Conflicting inline/header units, conflicting inline/row units, unpermitted unit suffixes, multi-token phrases, ranges (`0.22..0.47 uF`), tolerances (`±5 %`), straddling words, and overlapping intra-cell word boxes all fail closed in Release builds.

### Negative / Trade-offs

- **Operator configuration required**: The operator must explicitly list the permitted unit symbols for each inline value column in `inline_value_unit_bindings`.
- **Does not solve P3 (merged cells / hierarchical row context)**: Tables with vertically spanned dielectric labels (`VJ MLCC` full table), horizontally spanned temperature cells (`MRS16 / MRS25` full table), or parent header rows (`Insulation voltage:` in `MRS16`) remain out of scope for Phase 4G.

