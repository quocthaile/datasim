# Datasim — Phase 4G Validation Report: Deterministic Inline Value/Unit Parsing

## 1. Validation Summary

Phase 4G (`feat/phase-4g-inline-value-unit-parsing`) was validated on local Windows x64 (`MSYS2 UCRT64 GCC 16.2.0`, `Poppler 26.10.0`, `ngspice-47`, `Python 3.14.7`) against both:

1. **In-Repository Synthetic Regression Suites** (all 14 required Phase 4G inline value/unit cases plus Phase 1–4F non-regression suites, `9/9` CTest targets passing with `0` skipped tests, `113` assertions in `pdf_extraction_tests`, `7/7` unittest methods in `test_pdf_corpus_evaluator.py`, and `5/5` CI runtime tests in `tests/ci`).
2. **Untracked Local Real-Datasheet Pilot Corpus (`build/corpus/`)** across three distinct evaluation baselines:
   - **Baseline 1**: Phase 4E original configuration and ground truth (`15` observations).
   - **Baseline 2**: Phase 4F header-bound configuration and adjudicated ground truth (`15` observations, plus comparison on the `22`-observation P2-expanded subset).
   - **Baseline 3**: Phase 4G inline-enabled configuration and versioned ground truth on both the **identical `15`-observation subset** (`TP = 14/15, F1 = 0.9333`) and the **P2-expanded `22`-observation subset** (`TP = 20/22, F1 = 0.9091`, where Phase 4F without inline parsing drops to `TP = 13/18, Recall = 13/22 = 0.5909, F1 = 0.6500`).

All candidate-only boundary invariants (`ReviewState::candidate`, `review_history.empty()`, `knowledge == std::nullopt`, `models.empty()`) and `4/4` fail-closed negative controls hold without regression.

---

## 2. Phase 4F Baseline Verification (Step 1)

- **PR #10 (`feat/phase-4f-explicit-header-binding`) Merge Commit on `main`**: `9f3c115d13a40204d3d7f83382a50c107b14b803` (merging PR #10 head `049fc0692276de10e2a680b589076d9a7de07bfa`).
- **Post-Merge Hosted Windows CI Run on `main`**: [`42044651168`](https://github.com/quocthaile/datasim/actions/runs/42044651168) (`conclusion: success`, all build, unit, Poppler PDF, ngspice-47, and corpus evaluator steps green).
- **Historical Synthetic Fixture Checksums (Unchanged)**:
  - `tests/fixtures/synthetic_datasheet.pdf`: `7133e8615d2c59843cf780a503f3eb900ba9850b0750a888ebfa4b9b5955754e`
  - `tests/fixtures/image_only_datasheet.pdf`: `82a4c51cabcb33638a1126498b8bb55a2733772f6653ba379fa16420e4d6a564`
  - `tests/fixtures/synthetic_header_binding_datasheet.pdf`: `505ca09bc45072b639b28ef1002a96cb63224302f9e23994355057609cfa9470`
- **New Phase 4G Synthetic Fixture Checksum**:
  - `tests/fixtures/synthetic_inline_unit_datasheet.pdf`: `f3022a2fd8a498961a89f7d62030ac66d7d427fc290b4201fcfcba58eebc861f` (`4055` bytes)

---

## 3. Synthetic Regression Validation — Cases 1–14 (Step 8)

Verified in Release builds by [`tests/unit/pdf_extraction_tests.cpp`](../tests/unit/pdf_extraction_tests.cpp) (`113` checks) and [`tests/evaluation/test_pdf_corpus_evaluator.py`](../tests/evaluation/test_pdf_corpus_evaluator.py) (`7` unittest methods) using [`tests/fixtures/synthetic_inline_unit_datasheet.pdf`](../tests/fixtures/synthetic_inline_unit_datasheet.pdf):

| Case | Source Cell / Configuration | Expected Behavior | Verified Result |
| --- | --- | --- | --- |
| **Case 1** | `200 V` (`TABLE_INLINE_MAIN` row 1, `typ`) | `original_value_text = "200"`, `original_unit_symbol = "V"`, `parsed_numeric_value = 200.0`, `raw_cell_text = "200 V"` | **PASS** |
| **Case 2** | `0.4 W` (`TABLE_INLINE_MAIN` row 2, `typ`) | `original_value_text = "0.4"`, `original_unit_symbol = "W"`, `parsed_numeric_value = 0.4`, `raw_cell_text = "0.4 W"` | **PASS** |
| **Case 3** | `220 pF` (`TABLE_INLINE_MAIN` row 3, `min`) | `original_value_text = "220"`, `original_unit_symbol = "pF"`, `parsed_numeric_value = 220.0`, `raw_cell_text = "220 pF"` | **PASS** |
| **Case 4** | `10 nF` (`TABLE_INLINE_MAIN` row 3, `max`) | `original_value_text = "10"`, `original_unit_symbol = "nF"`, `parsed_numeric_value = 10.0`, `raw_cell_text = "10 nF"` | **PASS** |
| **Case 5** | `200 V` with consistent `HeaderUnitBinding` `VALUE (V)` (`TABLE_INLINE_HDR_OK`) | Succeeds; inline unit `"V"` consistent with header `"V"`, `parsed_numeric_value = 200.0`, `raw_cell_text = "200 V"` | **PASS** |
| **Case 6** | `200 V` with conflicting `HeaderUnitBinding` `VALUE (mV)` (`TABLE_INLINE_HDR_CONFLICT`) | Fails closed (`candidate_bundle == std::nullopt`) with `conflicts with bound header unit 'mV'` diagnostic | **PASS** |
| **Case 7** | Missing unit (`55` bare scalar in `TABLE_INLINE_MAIN` row 4, `typ`) | `original_value_text = "55"`, `original_unit_symbol = std::nullopt`, `parsed_numeric_value = std::nullopt`, `blank unit left unresolved` diagnostic | **PASS** |
| **Case 8** | Ambiguous number (`1,000 V` in `TABLE_INLINE_MAIN` row 5, `typ`) | Preserves `original_value_text = "1,000"`, `original_unit_symbol = "V"`, `raw_cell_text = "1,000 V"`, `parsed_numeric_value = std::nullopt`, locale ambiguity diagnostic | **PASS** |
| **Case 9** | Range (`0.22..0.47 uF` in `TABLE_INLINE_MAIN` row 6, `typ`) | Preserves `original_value_text = "0.22..0.47"`, `original_unit_symbol = "uF"`, `raw_cell_text = "0.22..0.47 uF"`, `parsed_numeric_value = std::nullopt` | **PASS** |
| **Case 10** | Footnote-marked source text (`200 V*` and `75* V` in `TABLE_INLINE_MAIN` rows 7–8) | Splits `"200"` / `"V"` and `"75"` / `"V"`, records `footnote_refs = {"*"}`, leaves `parsed_numeric_value = std::nullopt`, preserves `raw_cell_text = "200 V*"` / `"75* V"` | **PASS** |
| **Case 11** | Two adjacent inline columns (`min` `220 pF` at `x=218..295`, `max` `10 nF` at `x=305..385`) with narrow geometry | Both columns extracted deterministically on the same row without word cross-assignment | **PASS** |
| **Case 12** | Overlapping column spans, straddling words, unallowed unit token, or intra-cell overlapping word bounding boxes (`Page 2`) | All fail closed (`candidate_bundle == std::nullopt`) with explicit geometry/unit diagnostics | **PASS** |
| **Case 13** | Unicode unit preservation (`1.0 µF`, `\xC2\xB5F` on `Page 2` `TABLE_INLINE_UNICODE`) | Preserves `original_unit_symbol = "\xC2\xB5F"` and `raw_cell_text = "1.0 \xC2\xB5F"` verbatim without ASCII conversion | **PASS** |
| **Case 14** | Legacy Phase 4F behavior with `inline_value_unit_bindings` empty on `TABLE_INLINE_MAIN` | Does not split `"200 V"`; leaves `original_value_text = "200 V"`, `original_unit_symbol = std::nullopt`, `parsed_numeric_value = std::nullopt` | **PASS** |

---

## 4. Real-Datasheet Pilot Corpus Re-Evaluation (Step 9)

### 4.1 Real-Datasheet P2 Geometry Analysis

Inspection of the actual Poppler `pdftotext -bbox-layout` word coordinates across the untracked local real-datasheet corpus (`build/corpus/`) confirmed why inline value/unit parsing (`InlineValueUnitBinding`) is required:

1. **`vishay_mrs16_mrs25.pdf` (`REAL-CASE-01`, Page 1)**:
   - On the 4 parameter rows of `MRS16`:
     - `Rated dissipation, P70`: `"0.4"` (`x = 290.066..301.309`), `"W"` (`x = 304.499..312.999`)
     - `Operating voltage, Umax AC/DC`: `"200"` (`x = 291.258..304.742`), `"V"` (`x = 307.932..313.947`)
     - `Insulation voltage: 1 min; Uins`: `"300"` (`x = 291.258..304.742`), `"V"` (`x = 307.932..313.947`)
     - `Dielectric withstanding voltage of the coating`: `"75"` (`x = 293.504..302.496`), `"V"` (`x = 304.683..310.697`)
   - Because `"200"` and `"300"` extend to `xMax = 304.742` while `"W"` (`xMin = 304.499`) and `"V"` on `"75 V"` (`xMin = 304.683`) start to the left of `304.742`, **no horizontal split `x` exists** that separates values from units across all 4 rows. Phase 4E/4F could only extract a 2-row sub-band (`y = 490..565`) using a `305.5..306.0` split. With Phase 4G `InlineValueUnitBinding{typical, {"W", "V"}}` on `typical_column = [285.0, 320.0]`, all 4 rows are extracted cleanly without any artificial value/unit split.
2. **`vishay_crcw_e3.pdf` (`REAL-CASE-03`, Page 1)**:
   - `"150"` ends at `xMax = 404.098` while `"V"` on `"50 V"` starts at `xMin = 404.100`, forcing Phase 4E/4F to use a `0.001 pt` column boundary (`typical_column = [390.0, 404.098]`, `unit_column = [404.099, 420.0]`). With Phase 4G `InlineValueUnitBinding{typical, {"V"}}` on `typical_column = [390.0, 420.0]`, the sub-point split is eliminated.
3. **`vishay_vj_commercial_mlcc.pdf` (`REAL-CASE-05`, Page 2)**:
   - Both `MINIMUM` (`"1.0 pF"`, `"56 pF"`) and `MAXIMUM` (`"220 pF"`, `"820 pF"`, `"4.7 nF"`, `"10 nF"`, `"12 nF"`) encode inline value+unit pairs in separate columns (`x = 390..426` and `x = 450..490`). Phase 4E/4F could only extract `MINIMUM` (via a `0.43 pt` gap at `x = 407.0`) because `ElectricalTableLayout` has only one `unit_column` and `MAXIMUM` mixes `pF` and `nF` in the same column. With Phase 4G `InlineValueUnitBinding` on both `minimum` (`{"pF"}`) and `maximum` (`{"pF", "nF"}`), all 10 observations (`5` `MINIMUM` + `5` `MAXIMUM`) are extracted with `10/10` `TP` (`F1 = 1.0000`).

### 4.2 Separation of Parser Logic, Operator Geometry, and Ground-Truth Annotations

To maintain strict scientific comparability, three baselines were evaluated on the **identical 15-observation subset**, followed by a fourth comparison on the **P2-expanded 22-observation subset**:

1. **Baseline 1 — Phase 4E Original** ([`evaluation/real_corpus_manifest.json`](../evaluation/real_corpus_manifest.json), `SHA-256: a8298765b054d029ad71d4df12ab1021402940259f6557366f58582611c393da` vs. `build/corpus/real_ground_truth.json`, `SHA-256: 5c585ef7e87570a1477ac84dbac4dcec46330b04c613232ff7ef8a5691ad797a`): No header bindings, no inline value/unit bindings.
2. **Baseline 2 — Phase 4F Header-Bound** ([`evaluation/real_corpus_manifest_phase4f.json`](../evaluation/real_corpus_manifest_phase4f.json), `SHA-256: ab3d2eb64aafac020c1ec9060cfee04a75f2f1cfb8f0e9a8269a52c131b7b384` vs. `build/corpus/real_ground_truth_phase4f.json`, `SHA-256: a195e6e6b2d8c28d917b3897288cbbc941a22944be714becd52f9216578ecd27`): Adds `HeaderConditionBinding` on `REAL-CASE-05` and `HeaderUnitBinding` on `REAL-CASE-07`, still relying on sub-point `unit_column` splits on `REAL-CASE-01`, `REAL-CASE-03`, and `REAL-CASE-05`.
3. **Baseline 3a — Phase 4G Inline-Enabled on Identical 15-Observation Subset** ([`evaluation/real_corpus_manifest_phase4g.json`](../evaluation/real_corpus_manifest_phase4g.json), `SHA-256: a29a610942006d8cc58c067476a29d0526a4ab257178fd092ee2c84114670cb2` vs. `build/corpus/real_ground_truth_phase4g.json`, `SHA-256: 6f8b4ade5d5ae6b7062db902805fcbbec75a13b2f2010601f6f359a2d57a8bf1`): Replaces the separate `unit_column` splits on `REAL-CASE-01`, `REAL-CASE-03`, and `REAL-CASE-05` with single-column `InlineValueUnitBinding` (`typical = ["V"]` on `REAL-CASE-01` and `REAL-CASE-03`; `minimum = ["pF"]` on `REAL-CASE-05`) and verifies `expected_raw_cell_text` (`"200 V"`, `"300 V"`, `"150 V"`, `"50 V"`, `"1.0 pF"`, `"56 pF"`) on all 10 inline observations.

### 4.3 Three-Baseline Comparison on Identical 15-Observation Subset

| Metric / Indicator | Baseline 1: Phase 4E (`15` obs) | Baseline 2: Phase 4F (`15` obs) | Baseline 3a: Phase 4G (`15` obs, inline-enabled) |
| --- | --- | --- | --- |
| **True Positives (`TP`)** | `9` | `14` | **`14`** |
| **False Positives (`FP`)** | `6` | `1` | **`1`** |
| **False Negatives (`FN`)** | `6` | `1` | **`1`** |
| **Precision** | `0.6000 (9/15)` | `0.9333 (14/15)` | **`0.9333 (14/15)`** |
| **Recall** | `0.6000 (9/15)` | `0.9333 (14/15)` | **`0.9333 (14/15)`** |
| **F1 Score** | `0.6000 (18/30)` | `0.9333 (28/30)` | **`0.9333 (28/30)`** |
| `raw_value` agreement (incl. `expected_raw_cell_text` in 4G) | `1.0000 (15/15)` | `1.0000 (15/15)` | **`1.0000 (15/15)`** |
| `original_unit` agreement | `0.6667 (10/15)` | `1.0000 (15/15)` | **`1.0000 (15/15)`** |
| `parsed_scalar` agreement | `0.6667 (10/15)` | `1.0000 (15/15)` | **`1.0000 (15/15)`** |
| `parameter_label` agreement | `0.9333 (14/15)` | `0.9333 (14/15)` | **`0.9333 (14/15)`** (1 P3 row in `MRS16`) |
| `operating_conditions` agreement | `1.0000 (15/15)` | `1.0000 (15/15)` | **`1.0000 (15/15)`** |
| `evidence_location` agreement | `1.0000 (15/15)` | `1.0000 (15/15)` | **`1.0000 (15/15)`** |
| `footnote_refs` agreement | `1.0000 (15/15)` | `1.0000 (15/15)` | **`1.0000 (15/15)`** |
| **Sub-Point `unit_column` Splits Required** | `3` (`0.5 pt`, `0.001 pt`, `0.43 pt`) | `3` (`0.5 pt`, `0.001 pt`, `0.43 pt`) | **`0` (eliminated via `InlineValueUnitBinding`)** |
| **Unsupported Layouts Fail-Closed** | `1.0000 (4/4)` | `1.0000 (4/4)` | **`1.0000 (4/4)`** |
| **Repeat-Run Determinism** | `8/8` (`0` mismatches) | `8/8` (`0` mismatches) | **`8/8` (`0` mismatches)** |
| **Candidate-Only Boundary Verified** | `true` (`15/15`) | `true` (`15/15`) | **`true` (`15/15`)** |

### 4.4 P2-Expanded 22-Observation Subset Comparison (Phase 4F vs. Phase 4G)

When the evaluation subset is expanded to include the previously unextractable P2 inline cells in `REAL-CASE-01` (`0.4 W` and `75 V`, expanding `y` to `[456.0, 595.0]`) and `REAL-CASE-05` (`MAXIMUM` column `220 pF`, `820 pF`, `4.7 nF`, `10 nF`, `12 nF`, expanding `xMax` to `500.0`) — totaling `22` ground-truth observations across the same 8 cases:

| Metric / Indicator | Phase 4F Without Inline Binding (`22` obs subset) | Phase 4G With `InlineValueUnitBinding` (`22` obs subset) | Delta (Phase 4G vs. Phase 4F) |
| --- | --- | --- | --- |
| **True Positives (`TP`)** | `13` | **`20`** | **`+7 TP`** |
| **False Positives (`FP`)** | `5` (`VJ MLCC` `MAXIMUM` cells unsplit) | **`2`** (only P3 child rows in `MRS16`) | **`-3 FP`** |
| **False Negatives (`FN`)** | `9` (`4` `MRS16` fail-closed + `5` `VJ MLCC` `MAXIMUM`) | **`2`** (only P3 child rows in `MRS16`) | **`-7 FN`** |
| **Precision** | `0.7222 (13/18)` | **`0.9091 (20/22)`** | **`+0.1869`** |
| **Recall** | `0.5909 (13/22)` | **`0.9091 (20/22)`** | **`+0.3182`** |
| **F1 Score** | `0.6500 (26/40)` | **`0.9091 (40/44)`** | **`+0.2591`** |
| `raw_value` agreement | `0.7222 (13/18)` | **`1.0000 (22/22)`** | **`+9` matched (`100%`)** |
| `original_unit` agreement | `0.8333 (15/18)` | **`1.0000 (22/22)`** | **`+7` matched (`100%`)** |
| `parsed_scalar` agreement | `0.7222 (13/18)` | **`1.0000 (22/22)`** | **`+9` matched (`100%`)** |
| `parameter_label` agreement | `1.0000 (18/18)` (`MRS16` failed closed) | **`0.9091 (20/22)`** (`2` P3 rows in `MRS16`) | `+2` extracted |
| `REAL-CASE-05-VJ-MLCC` (`MIN` + `MAX` `10` obs) | `5/10 TP (F1 = 0.5000)` | **`10/10 TP (F1 = 1.0000)`** | **`+5/10 TP`** |
| **Unsupported Layouts Fail-Closed** | `1.0000 (4/4)` | **`1.0000 (4/4)`** | Unchanged (`0` false accepts) |

---

## 5. Remaining Out-of-Scope Limitations

Phase 4G resolves **P2 (Inline Value/Unit Cells)** without weakening geometric safety or broadening the numeric grammar. The following real-datasheet layout challenges remain intentionally out of scope for subsequent phases:

1. **P3 — Hierarchical Parent-Row & Merged-Cell Propagation**:
   - In `REAL-CASE-01-MRS16-VOLTAGE-BAND`, the parent heading `'Insulation voltage:'` (`y = 544.5`) spans visually above child rows `'1 min; U ins'` (`300 V`) and `'continuous'` (`75 V`). Without P3 parent-row label composition, those child rows emit only their row-local parameter cell text (`"1 min; U ins"`, `"continuous"`).
   - Multi-row vertically merged parameter/unit cells (`REAL-CASE-02`, `REAL-CASE-04`, `REAL-CASE-06`) and multi-column sub-table blocks (`REAL-CASE-08`) continue to fail closed (`4/4`).
2. **P4 — Subscript/Superscript Baseline Coalescing**:
   - Lowered subscript glyphs (`P70`, `Umax`, `Uins` in `MRS16`) remain separated by `row_y_tolerance` when their vertical baseline offset exceeds the row clustering threshold.
3. **Non-Numeric Condition Codes**:
   - Package/case code `"A"` in `REAL-CASE-07-150D-RATINGS-SUBBAND` remains safely unresolved in `raw_conditions_text`.

---

## 6. Non-Regression Verification (Step 10)

All existing Phase 1–4F build, unit, integration, and CI contract suites were executed in `build/phase4g` (`Release`):

- `ctest --test-dir build/phase4g --output-on-failure -V`: **`9/9` passed** (`unit`, `component_model`, `vertical_slice`, `intake`, `intake_handoff`, `pdf_extraction`, `backend_errors`, `ngspice_rc`, `pdf_corpus_evaluation`), `0` skipped.
- `python -m unittest discover -s tests/ci -v`: **`5/5` passed**.
- `python -m unittest discover -s tests/evaluation -v`: **`7/7` passed**.
- `build/phase4g/rc_study.exe ngspice_con build/phase4g/rc_study_out`: Executed and verified within the 1% ideal-RC step-response tolerance.

