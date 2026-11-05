# Datasim — Phase 4F Validation Report: Explicit Header Unit and Condition Binding

## 1. Validation Summary

Phase 4F (`feat/phase-4f-explicit-header-binding`) was validated on local Windows x64 (`MSYS2 UCRT64 GCC 16.2.0`, `Poppler 26.10.0`, `ngspice-47`, `Python 3.14.3`) against both:

1. **In-Repository Synthetic Regression Suites** (Cases A–H plus Phase 1–4E non-regression suites, `9/9` CTest targets passing with `0` skipped tests).
2. **Untracked Local Real-Datasheet Pilot Corpus (`build/corpus/`)** (comparing the unmodified Phase 4E baseline against the Phase 4F header-bound configuration).

All candidate-only boundary invariants (`ReviewState::candidate`, `review_history.empty()`, `knowledge == std::nullopt`, `models.empty()`) and `4/4` fail-closed negative controls hold without regression.

---

## 2. Phase 4E Baseline Verification (Step 1 & Step 2)

- **PR #9 Merge Commit on `main`**: `b45c32224e0316dd9f93b6e9bfc3ab89cd3ceaf4`
- **Post-Merge Hosted Windows CI Run on `main`**: `42037281172` (`success`, all jobs green)
- **Data Governance Check**: Verified that zero vendor PDFs (`*.pdf` outside `tests/fixtures/`) and zero copyrighted vendor ground-truth passages are tracked in Git.

### Attribution of Phase 4E Real-Datasheet Discrepancies

In the Phase 4E pilot baseline (`TP = 9, FP = 6, FN = 6, Precision = 0.6000, Recall = 0.6000, F1 = 0.6000`):

1. **5 of the 6 `FP/FN` observation mismatches** (`REAL-CASE-07-150D-RATINGS-SUBBAND`, `vishay_150d_tantalum.pdf` Page 2, rows 1–5) were caused by **P1 (Header-Declared Units)**: the capacitance unit `(μF)` is declared in the column header `CAPACITANCE AT +25 °C 120 Hz (μF)` (`x = 70.98..128.84, y = 331.46..365.77`) while the per-row unit cells are empty.
2. **1 of the 6 `FP/FN` observation mismatches** (`REAL-CASE-01-MRS16-VOLTAGE-BAND`, `vishay_mrs16_mrs25.pdf` Page 1, row 5) is caused by **P3 (Hierarchical Parent-Row Propagation)**: parent row `'Insulation voltage:'` on row 4 (`y = 544.5`) above child row `'1 min; U ins'` on row 5 (`y = 559.5`). This is not a column-header binding issue and remains intentionally out of scope for Phase 4F.
3. **5 of the 10 `condition_mismatch` taxonomy counts** (`REAL-CASE-05-VJ-MLCC-C0G-MIN-BAND`, `vishay_vj_commercial_mlcc.pdf` Page 2, rows 1–5) were caused by **Condition-Header Unit Declaration**: the column header `MAXIMUM VOLTAGE (V)` (`x = 265.44..346.06, y = 109.60..127.75`) declares parameter `VOLTAGE` and unit `(V)`, while the row cells contain bare numeric strings (`"100"`, `"250"`, `"500"`, `"630"`, `"630"`).
4. **The remaining 5 `condition_mismatch` taxonomy counts** (`REAL-CASE-07-150D-RATINGS-SUBBAND`, rows 1–5) are non-numeric package `CASE CODE` cells (`"A"`), which must remain unresolved in `raw_conditions_text`.

---

## 3. Synthetic Regression Validation — Cases A–H (Step 6)

Verified in Release builds by [`tests/unit/pdf_extraction_tests.cpp`](../tests/unit/pdf_extraction_tests.cpp) (`72` assertions) and [`tests/evaluation/test_pdf_corpus_evaluator.py`](../tests/evaluation/test_pdf_corpus_evaluator.py) (`6` unittest methods) using [`tests/fixtures/synthetic_header_binding_datasheet.pdf`](../tests/fixtures/synthetic_header_binding_datasheet.pdf) (`SHA-256: 505ca09bc45072b639b28ef1002a96cb63224302f9e23994355057609cfa9470`):

| Case | Description | Expected Behavior | Verified Result |
| --- | --- | --- | --- |
| **Case A** | Unit only in column header (`Capacitance (uF)`, row unit blank) | Binds `"uF"`, parses `0.22`, attaches `header_unit;column=typ` evidence ref | **PASS** |
| **Case B** | Explicit row unit (`"uF"`) consistent with header unit (`"uF"`) | Row unit takes precedence; no redundant header unit evidence ref attached | **PASS** |
| **Case C** | Row unit (`"nF"`) conflicts with bound header unit (`"uF"`) | Fails closed (`candidate_bundle == std::nullopt`) with conflict diagnostic | **PASS** |
| **Case D** | Mismatched header text (`Capacitance (pF)`), empty header box, or straddling word | Fails closed (`candidate_bundle == std::nullopt`) with header diagnostic | **PASS** |
| **Case E** | Valid condition header (`VOLTAGE (V)`) + bare numeric condition (`"100"`) | Parses `OperatingCondition{VOLTAGE, "100", "V", 100.0}` + attaches `header_condition;column=condition` evidence ref | **PASS** |
| **Case F** | Non-numeric condition code (`"A"`) with condition header bound | Leaves `operating_conditions` empty, preserves `raw_conditions_text = "A"`, emits unresolved diagnostic | **PASS** |
| **Case G** | Two value columns (`typ` in `uF`, `max` in `uA`) bound independently | Binds `0.22 uF` (`header_unit;column=typ`) and `0.5 uA` (`header_unit;column=max`) on the same row | **PASS** |
| **Case H** | Original Phase 4C table with no header bindings | Identical to Phase 4C–4E (`20/20` TP on `synthetic_corpus_manifest.json`, `9/9` on `TABLE_R`) | **PASS** |
| **Unicode** | Header unit `CAPACITANCE (µF)` (`\xC2\xB5F`) on Page 2 | Preserves `"\xC2\xB5F"` verbatim without ASCII conversion | **PASS** |

---

## 4. Real-Datasheet Pilot Benchmark: Phase 4E vs. Phase 4F (Step 7)

### 4.1 Operator Configuration Changes vs. Code Changes

To separate code changes from operator manifest configuration changes:

- **Unmodified Phase 4E Manifest** ([`evaluation/real_corpus_manifest.json`](../evaluation/real_corpus_manifest.json), `SHA-256: a8298765b054d029ad71d4df12ab1021402940259f6557366f58582611c393da`): Contains no `header_unit_bindings` or `header_condition_binding`. Running the Phase 4F binary on this manifest reproduces the exact Phase 4E baseline (`TP = 9, FP = 6, FN = 6, F1 = 0.6000`).
- **Phase 4F Header-Bound Manifest** ([`evaluation/real_corpus_manifest_phase4f.json`](../evaluation/real_corpus_manifest_phase4f.json), `SHA-256: ab3d2eb64aafac020c1ec9060cfee04a75f2f1cfb8f0e9a8269a52c131b7b384`):
  1. `REAL-CASE-05-VJ-MLCC-C0G-MIN-BAND`: Extended `region` top from `y = 140.0` to `108.0` (`[190.0, 108.0, 427.0, 215.0]`), set `header = [108.0, 141.0]`, and added `header_condition_binding` (`header_box = [265.0, 109.0, 347.0, 128.5]`, `parameter_name = "VOLTAGE"`, `unit_symbol = "V"`, `expected_header_text = "MAXIMUM VOLTAGE (V)"`).
  2. `REAL-CASE-07-150D-RATINGS-SUBBAND`: Extended `region` top from `y = 382.0` to `330.0` (`[70.0, 330.0, 360.0, 440.0]`), set `header = [330.0, 381.0]`, and added `header_unit_bindings` for `typical` (`header_box = [70.0, 331.0, 130.0, 366.5]`, `unit_symbol = "μF"`, `expected_header_text = "CAPACITANCE AT +25 °C 120 Hz (μF)"`).
  3. All other 6 cases (`REAL-CASE-01..04`, `06`, `08`) are unchanged.

### 4.2 Ground-Truth Notation Note (`"μF"` vs. ASCII `"uF"`)

In Phase 4E, `build/corpus/real_ground_truth.json` (`SHA-256: 5c585ef7e87570a1477ac84dbac4dcec46330b04c613232ff7ef8a5691ad797a`) was authored with ASCII `"uF"` for `REAL-CASE-07-150D-RATINGS-SUBBAND` and `"operating_conditions": []` for `REAL-CASE-05-VJ-MLCC-C0G-MIN-BAND`. Because `datasim_pdf` strictly preserves the exact PDF source unit (`"μF"`, UTF-8 `\xCE\xBC\x46`) without converting to ASCII `"uF"`, we report results against **both** the unmodified Phase 4E ground-truth file and `build/corpus/real_ground_truth_phase4f.json` (`SHA-256: a195e6e6b2d8c28d917b3897288cbbc941a22944be714becd52f9216578ecd27`), which records the exact PDF source unit `"μF"`, the bound `VOLTAGE` operating conditions on `VJ MLCC`, and the expected header evidence references (`header_unit;column=typ` and `header_condition;column=condition`).

### 4.3 Side-by-Side Real-Corpus Benchmark Comparison

| Metric / Indicator | Phase 4E Baseline (`real_corpus_manifest.json` vs. `real_ground_truth.json`) | Phase 4F Bound vs. Unmodified Phase 4E GT (`real_ground_truth.json`) | Phase 4F Bound vs. Exact-Source Phase 4F GT (`real_ground_truth_phase4f.json`) |
| --- | --- | --- | --- |
| **True Positives (`TP`)** | `9` | `9` (due to `"μF"` vs ASCII `"uF"` in P4E GT) | **`14`** (`+5`) |
| **False Positives (`FP`)** | `6` | `6` | **`1`** (`-5`) |
| **False Negatives (`FN`)** | `6` | `6` | **`1`** (`-5`) |
| **Precision** | `0.6000 (9/15)` | `0.6000 (9/15)` | **`0.9333 (14/15)`** |
| **Recall** | `0.6000 (9/15)` | `0.6000 (9/15)` | **`0.9333 (14/15)`** |
| **F1 Score** | `0.6000 (18/30)` | `0.6000 (18/30)` | **`0.9333 (28/30)`** |
| `raw_value` agreement | `1.0000 (15/15)` | `1.0000 (15/15)` | **`1.0000 (15/15)`** |
| `original_unit` agreement | `0.6667 (10/15)` | `0.6667 (10/15)` (`"μF"` vs `"uF"`) | **`1.0000 (15/15)`** |
| `parsed_scalar` agreement | `0.6667 (10/15)` | **`1.0000 (15/15)`** (`+5/15`) | **`1.0000 (15/15)`** (`+5/15`) |
| `parameter_label` agreement | `0.9333 (14/15)` | `0.9333 (14/15)` | **`0.9333 (14/15)`** |
| `operating_conditions` agreement | `1.0000 (15/15)` | `0.6667 (10/15)` (bound `VOLTAGE` vs `[]`) | **`1.0000 (15/15)`** |
| `evidence_location` agreement (incl. header refs) | `1.0000 (15/15)` | `1.0000 (15/15)` | **`1.0000 (15/15)`** |
| `footnote_refs` agreement | `1.0000 (15/15)` | `1.0000 (15/15)` | **`1.0000 (15/15)`** |
| **Unsupported Layouts Fail-Closed** | `1.0000 (4/4)` | `1.0000 (4/4)` | **`1.0000 (4/4)`** |
| **False Accepts** | `0` | `0` | **`0`** |
| **Taxonomy: `unit_mismatch`** | `5` | `5` (`"μF"` vs `"uF"`) | **`0`** (`-5`) |
| **Taxonomy: `ambiguous_numeric`** | `10` | **`0`** (`-10`) | **`0`** (`-10`) |
| **Taxonomy: `condition_mismatch`** | `10` | `10` | **`5`** (`-5`, only `150D` code `'A'` remains) |
| **Repeat-Run Determinism** | `8/8` (`0` mismatches) | `8/8` (`0` mismatches) | **`8/8` (`0` mismatches)** |
| **Candidate-Only Boundary Verified** | `true` (`15/15`) | `true` (`15/15`) | **`true` (`15/15`)** |

---

## 5. Non-Regression Verification (Step 8)

All existing Phase 1–4E build, unit, integration, and CI contract suites were executed in `build/phase4f` (`Release`):

- `ctest --test-dir build/phase4f --output-on-failure`: **`9/9` passed** (`unit`, `component_model`, `vertical_slice`, `intake`, `intake_handoff`, `pdf_extraction`, `backend_errors`, `ngspice_rc`, `pdf_corpus_evaluation`), `0` skipped.
- `python -m unittest discover -s tests/ci -v`: **`8/8` passed**.
- `python -m unittest discover -s tests/evaluation -v`: **`6/6` passed**.
- `build/phase4f/rc_study.exe`: Executed and verified within the 1% ideal-RC step-response tolerance.
