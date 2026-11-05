# Phase 4E Validation Report — Real Datasheet Evaluation Corpus

- **Phase**: 4E
- **Status**:
  - **Synthetic Corpus Regression Suite (`datasim-phase4e-synthetic-regression-v1`)**: **VERIFIED** locally and in hosted Windows CI (`pdf_corpus_evaluation`, PR #9 runs [`42031281489`](https://github.com/quocthaile/datasim/actions/runs/42031281489), [`42031409931`](https://github.com/quocthaile/datasim/actions/runs/42031409931) and post-merge `main` run [`42037281172`](https://github.com/quocthaile/datasim/actions/runs/42037281172))
  - **Real-Datasheet Pilot Corpus (`datasim-phase4e-real-datasheet-pilot-v1`)**: **VERIFIED locally** on Windows x64 against 5 untracked manufacturer PDFs in `build/corpus/` (not executed in hosted CI due to third-party PDF redistribution restrictions)
- **Baseline `main` Commit**: `69aba4df7ff752355a40e394de5d05ac988df34b` (PR #8 merge of Phase 4D; post-merge hosted Windows CI run [`42025406013`](https://github.com/quocthaile/datasim/actions/runs/42025406013) `success`)
- **Merged Commit on `main`**: `b45c32224e0316dd9f93b6e9bfc3ab89cd3ceaf4` (PR #9; post-merge hosted Windows CI run [`42037281172`](https://github.com/quocthaile/datasim/actions/runs/42037281172) `success`)
- **Branch**: `feat/phase-4e-datasheet-evaluation-corpus`

## 1. Environment and Artifact Checksums

### 1.1 Toolchain and Runtime Versions

| Component | Version / Path |
| --- | --- |
| Operating System | Windows 10/11 x64 (`win32`) |
| C++ Compiler | MSYS2 UCRT64 GCC `16.2.0` (`C:/msys64/ucrt64/bin/c++.exe`) |
| Build System | CMake `4.3.2` + Ninja |
| Poppler `pdfinfo` | `pdfinfo version 26.10.0` (`C:/msys64/ucrt64/bin/pdfinfo.exe`) |
| Poppler `pdftotext` | `pdftotext version 26.10.0` (`C:/msys64/ucrt64/bin/pdftotext.exe`) |
| SPICE Backend | `ngspice-47` (`C:/msys64/ucrt64/bin/ngspice_con.exe`) |
| Python Interpreter | Python `3.13.12` (MSYS2 UCRT64) / `3.14.3` (CTest discovery) |

### 1.2 Manifest, Annotation, and PDF SHA-256 Digests

| Artifact Path | Tracking Status | SHA-256 Digest |
| --- | --- | --- |
| [`tests/evaluation/synthetic_corpus_manifest.json`](../tests/evaluation/synthetic_corpus_manifest.json) | Tracked in Git | `67dd99e030c446db6980134e4279207a1a66f1e77907fb75e469e4210ea2d24d` |
| [`tests/evaluation/synthetic_ground_truth.json`](../tests/evaluation/synthetic_ground_truth.json) | Tracked in Git | `e548509a0e2343b7f8a068ef2b638f229342e536427b74c059e6bd0b60dccb16` |
| [`evaluation/real_corpus_manifest.json`](../evaluation/real_corpus_manifest.json) | Tracked in Git | `ec25a3f8770db84d2224a2a2bf79d36061bce0b7fadb59748c056e49d77b2577` |
| `build/corpus/real_ground_truth.json` | Untracked local-only (`build/`) | `5c585ef7e87570a1477ac84dbac4dcec46330b04c613232ff7ef8a5691ad797a` |
| [`tests/fixtures/synthetic_datasheet.pdf`](../tests/fixtures/synthetic_datasheet.pdf) | Tracked in Git | `7133e861225318b47982dbf25ee2bb68a55ee5c11aa054675214515ab769cbb8` |
| [`tests/fixtures/image_only_datasheet.pdf`](../tests/fixtures/image_only_datasheet.pdf) | Tracked in Git | `82a4c51c353842facc8724e0380846c6a70adc798038963ce1a1c272a4a5cbab` |
| `build/corpus/vishay_mrs16_mrs25.pdf` | Untracked local-only (`build/`) | `645bb41aaa34f2a72fc19fc71d02ddb14ed400ae97634684ad1745189505f47e` |
| `build/corpus/vishay_crcw_e3.pdf` | Untracked local-only (`build/`) | `a10e1cf2bd4f0f6ef85de250077adb463cae243c2e8e0509354e09f050d24a92` |
| `build/corpus/vishay_vj_commercial_mlcc.pdf` | Untracked local-only (`build/`) | `6c9b40c81d312ba6dbe9d0a4e687ae18185efeebf8cb98e6d709020efa550211` |
| `build/corpus/vishay_150d_tantalum.pdf` | Untracked local-only (`build/`) | `197eeaedf55a47633246a565a73c8c078cc7da3a7246a472f0db4a48dbf523a7` |
| `build/corpus/vishay_mkp1840m_film.pdf` | Untracked local-only (`build/`) | `11cdd98ae202acb7dfaf013ff4d98e5b4c4a5abd8e1330dbf8471febe9958fe0` |

## 2. Commands Executed

```powershell
$env:PATH = "C:\msys64\ucrt64\bin;C:\msys64\usr\bin;" + $env:PATH

# 1. Clean Release build including datasheet_eval_dump and all test targets
cmake -S . -B build/phase4e -G Ninja -DCMAKE_BUILD_TYPE=Release -DDATASIM_NGSPICE="C:/msys64/ucrt64/bin/ngspice_con.exe"
cmake --build build/phase4e

# 2. Run full CTest suite (9/9 suites, 0 skips)
ctest --test-dir build/phase4e --output-on-failure

# 3. Run synthetic corpus evaluation
python scripts/eval/evaluate_pdf_corpus.py `
  --manifest tests/evaluation/synthetic_corpus_manifest.json `
  --annotations tests/evaluation/synthetic_ground_truth.json `
  --eval-dump-bin build/phase4e/datasheet_eval_dump.exe `
  --output-json build/phase4e/synthetic_eval_report.json `
  --output-md build/phase4e/synthetic_eval_summary.md

# 4. Run local real-datasheet pilot corpus evaluation
python scripts/eval/evaluate_pdf_corpus.py `
  --manifest evaluation/real_corpus_manifest.json `
  --annotations build/corpus/real_ground_truth.json `
  --corpus-dir build/corpus `
  --eval-dump-bin build/phase4e/datasheet_eval_dump.exe `
  --output-json build/phase4e/real_eval_report.json `
  --output-md build/phase4e/real_eval_summary.md

# 5. Run Python CI and evaluation unit tests + Phase 1.5 convergence study
python -m unittest discover -s tests/ci -v
python -m unittest discover -s tests/evaluation -v
./build/phase4e/rc_study.exe "C:/msys64/ucrt64/bin/ngspice_con.exe" build/phase4e/study
```

## 3. Synthetic Regression Benchmark Results (`datasim-phase4e-synthetic-regression-v1`)

- **Eligible Documents**: `2/2` (`doc.synthetic.rc`, `doc.synthetic.image_only`)
- **Configured Cases**: `7` (`1` `supported_single_page_table`, `1` `partially_supported_ambiguous`, `4` `unsupported_complex_layout`, `1` `image_only_or_scanned`)
- **Observation-Level Metrics (`SYN-CASE-01-TABLE-R` + `SYN-CASE-02-TABLE-C`)**:
  - `TP = 20`, `FP = 0`, `FN = 0`
  - `Precision = 1.0000 (20/20)`
  - `Recall = 1.0000 (20/20)`
  - `F1 = 1.0000 (40/40)`
- **Field-Level Agreement (`20/20` matched observations)**:
  - `raw_value`: `1.0000 (20/20)`
  - `original_unit`: `1.0000 (20/20)`
  - `qualifier`: `1.0000 (20/20)`
  - `parameter_label`: `1.0000 (20/20)`
  - `operating_conditions`: `1.0000 (20/20)`
  - `evidence_location`: `1.0000 (20/20)`
  - `footnote_refs`: `1.0000 (20/20)`
  - `parsed_scalar`: `1.0000 (20/20)` (including exact preservation of `null` on `"1,000"`, `"1.0*"`, and the three blank-unit rows)
- **Fail-Closed Cases (`SYN-CASE-03..07`)**: `5/5` failed closed as expected (`0` false accepts, `5/5` diagnostic-matched)
- **Reproducibility & Trust Boundary**: `7/7` repeat runs byte-identical; `29/29` inspected observations in `ReviewState::candidate`.

## 4. Real-Datasheet Pilot Corpus Benchmark Results (`datasim-phase4e-real-datasheet-pilot-v1`)

### 4.1 Section A — Corpus Eligibility and Layout Coverage

- **Total Candidate Documents Inspected**: `7`
- **Eligible Documents Evaluated Locally**: `5` (`2` resistor datasheets, `2` capacitor datasheets, `1` complex film-capacitor control datasheet)
- **Excluded Documents**: `2` (`doc.kemet.r82_film`, `doc.kemet.f862_film` — `tls_certificate_expired_on_official_source_host`)
- **Total Configured Table Cases**: `8`
  - `supported_single_page_table`: `1` (`REAL-CASE-03-CRCW-VOLTAGE-BAND`)
  - `partially_supported_ambiguous`: `3` (`REAL-CASE-01-MRS16-VOLTAGE-BAND`, `REAL-CASE-05-VJ-MLCC-C0G-MIN-BAND`, `REAL-CASE-07-150D-RATINGS-SUBBAND`)
  - `unsupported_complex_layout`: `4` (`REAL-CASE-02-MRS16-MRS25-FULL-TABLE`, `REAL-CASE-04-CRCW-TCR-MERGED-TABLE`, `REAL-CASE-06-VJ-MLCC-FULL-TABLE`, `REAL-CASE-08-MKP1840M-SPECIFIC-REF`)

### 4.2 Section B — Extraction Quality Conditional on Supported / Partially Supported Sub-Bands

| Metric | Value | Numerator / Denominator | Notes |
| --- | --- | --- | --- |
| True Positives (`TP`) | `9` | — | `1` on `MRS16`, `3` on `CRCW e3`, `5` on `VJ MLCC`, `0` on `150D` |
| False Positives (`FP`) | `6` | — | `1` on `MRS16` (child row label missing parent group header), `5` on `150D` (blank unit cell when unit is in column header) |
| False Negatives (`FN`) | `6` | — | Same `6` location-matched observations where a core field (`parameter_label` or `original_unit`/`parsed_scalar`) differed from ground truth |
| Precision | `0.6000` | `9 / 15` | Conditional on the 4 extractable sub-band cases (`15` total observations) |
| Recall | `0.6000` | `9 / 15` | Conditional on the 4 extractable sub-band cases (`15` total observations) |
| F1 Score | `0.6000` | `18 / 30` | Conditional on the 4 extractable sub-band cases |

#### Field-Level Agreement Across Matched Real-Datasheet Observations (`15` Matched Cells)

| Field | Matched | Total | Agreement Rate | Root Cause of Discrepancies |
| --- | --- | --- | --- | --- |
| `raw_value` | `15` | `15` | `1.0000 (15/15)` | All 15 extracted cells preserved exact vendor value strings (`200`, `300`, `50`, `75`, `150`, `1.0`, `56`, `0.22..0.47`). |
| `original_unit` | `10` | `15` | `0.6667 (10/15)` | In `vishay_150d_tantalum.pdf` (`REAL-CASE-07`), the unit `(uF)` is declared in the column header (`CAPACITANCE AT +25 °C 120 Hz (μF)`) rather than a per-row unit column, leaving `original_unit_symbol = null` on 5 rows. |
| `qualifier` | `15` | `15` | `1.0000 (15/15)` | All 15 cells matched their configured column qualifier (`minimum`, `typical`, `maximum`). |
| `parameter_label` | `14` | `15` | `0.9333 (14/15)` | In `vishay_mrs16_mrs25.pdf` (`REAL-CASE-01` Row 5), the parent label `Insulation voltage:` sits on Row 4 (`y=544.5`, no value cell), so Row 5 (`y=561.2`) captures only the indented child sub-label `1 min; U ins`. |
| `operating_conditions` | `15` | `15` | `1.0000 (15/15)` | Bare condition cells (`100`, `250`, `500`, `630` in `VJ MLCC`; `A` in `150D`) are preserved verbatim in `raw_conditions_text` with explicit `condition_mismatch` diagnostics. |
| `evidence_location` | `15` | `15` | `1.0000 (15/15)` | Zero-based `pdf_page_index`, `printed_page_label`, `table_or_figure_id`, and `row=<N>;column=<col>` matched on all 15 observations. |
| `footnote_refs` | `15` | `15` | `1.0000 (15/15)` | All 15 observations matched ground-truth footnote references. |
| `parsed_scalar` | `10` | `15` | `0.6667 (10/15)` | The 5 `150D` observations with header-only units properly withheld `parsed_numeric_value` (`null`) because the row unit cell was blank. |

### 4.3 Section C — Fail-Closed Behavior on Complex / Negative Vendor Tables

| Case ID | Document | Table Description | Expected | Actual | Diagnostic Summary |
| --- | --- | --- | --- | --- | --- |
| `REAL-CASE-02-MRS16-MRS25-FULL-TABLE` | `vishay_mrs16_mrs25.pdf` (p. 0) | Full `TECHNICAL SPECIFICATIONS` table with side-by-side `MRS16` / `MRS25` columns, inline units (`0.4 W`, `200 V`), and spanned temperature rows | `fail_closed` | `fail_closed_verified` (`0` false accepts) | `row 1 has a word outside or ambiguously across configured column spans: ±` |
| `REAL-CASE-04-CRCW-TCR-MERGED-TABLE` | `vishay_crcw_e3.pdf` (p. 1) | `TEMPERATURE COEFFICIENT AND RESISTANCE RANGE` table with multi-row merged parameter/TCR cells | `fail_closed` | `fail_closed_verified` (`0` false accepts) | `16` omitted rows with missing parameter cells followed by `selected region contained no supported min/typ/max value cells` |
| `REAL-CASE-06-VJ-MLCC-FULL-TABLE` | `vishay_vj_commercial_mlcc.pdf` (p. 1) | Full `QUICK REFERENCE DATA` table with separate inline units (`pF`, `nF`, `μF`) inside both `MINIMUM` and `MAXIMUM` columns | `fail_closed` | `fail_closed_verified` (`0` false accepts) | `row 1 has a word outside or ambiguously across configured column spans: DIELECTRIC` |
| `REAL-CASE-08-MKP1840M-SPECIFIC-REF` | `vishay_mkp1840m_film.pdf` (p. 1) | `SPECIFIC REFERENCE DATA` multi-frequency (`1 kHz`, `10 kHz`, `100 kHz`) and `dU/dt` matrix with superscript `10 -4` | `fail_closed` | `fail_closed_verified` (`0` false accepts) | `row 1 has a word outside or ambiguously across configured column spans: -4` |

- **Fail-Closed Rate**: `1.0000 (4/4)`
- **False Accept Count**: `0`

### 4.4 Section D — Reproducibility, Candidate-Only Boundary, and Error Taxonomy Breakdown

- **Repeated-Run Equivalence**: `8/8` cases produced byte-identical extraction JSON, observation IDs, ordering, and diagnostics across repeated invocations (`repeat_run_mismatches = 0`).
- **Candidate-Only Boundary**: `15/15` extracted real-datasheet observations remained in `ReviewState::candidate` with `review_history_count = 0`.

| Taxonomy Label | Synthetic Corpus Count | Real Pilot Corpus Count | Primary Cause in Real Pilot Corpus |
| --- | --- | --- | --- |
| `unsupported_layout` | `0` | `0` | Mapped to specific structural failure labels below |
| `missing_selectable_text` | `1` | `0` | All 5 acquired Vishay PDFs contain selectable text (`image_only_datasheet.pdf` tested in synthetic suite) |
| `invalid_table_region` | `1` | `1` | `REAL-CASE-04` (`selected region contained no supported min/typ/max value cells`) |
| `column_assignment_error` | `1` | `21` | `16` merged-cell rows in `REAL-CASE-04`, `3` unassigned words in `REAL-CASE-02/06/08`, `1` omitted valueless row + `1` hierarchical label mismatch in `REAL-CASE-01` |
| `row_segmentation_error` | `0` | `0` | Subscript baselines within sub-bands handled by `row_y_tolerance = 3.5`; superscript `-4` in `REAL-CASE-08` caught at column assignment |
| `missing_observation` | `0` | `0` | All 15 expected observations in extractable sub-bands were located |
| `spurious_observation` | `0` | `0` | Zero spurious or duplicate observations emitted |
| `raw_value_mismatch` | `0` | `0` | `15/15` matched observations preserved exact raw value text |
| `unit_mismatch` | `0` | `5` | `5` rows in `REAL-CASE-07` (`150D`) where unit `(μF)` is declared in the column header rather than a data-row unit column |
| `qualifier_mismatch` | `0` | `0` | `15/15` matched observations preserved exact column qualifier |
| `condition_mismatch` | `0` | `10` | `5` rows in `REAL-CASE-05` (`VJ MLCC` voltage `100..630` with unit `V` in column header) + `5` rows in `REAL-CASE-07` (`150D` case code `A`) |
| `evidence_location_mismatch` | `1` | `0` | Tested by `SYN-CASE-06-PAGE-OUT-OF-RANGE` |
| `ambiguous_numeric` | `5` | `10` | `5` blank-unit diagnostic warnings + `5` unparsed scalar comparisons on `REAL-CASE-07` (`150D`) |
| `source_identity_mismatch` | `1` | `0` | Tested by `SYN-CASE-07-MPN-MISMATCH` |
| `backend_or_environment_failure` | `0` | `0` | Tested by `TestPdfCorpusEvaluator::test_changed_or_missing_pdf_rejected` |
| `rights_or_document_unavailable` | `0` | `2` | `doc.kemet.r82_film` and `doc.kemet.f862_film` excluded due to expired TLS certificate on `content.kemet.com` |

## 5. Limitations and Prioritized Recommendations for Future Parser Phases

### 5.1 Current Evaluation Limitations

1. **Single-annotator ground truth**: All annotations in `tests/evaluation/synthetic_ground_truth.json` and `build/corpus/real_ground_truth.json` were authored and verified by a single engineering annotator (`annotator_count = 1`, `inter_annotator_agreement_claimed = false`). No inter-annotator kappa or multi-reviewer consensus is claimed.
2. **Untracked local vendor PDFs**: Because standard manufacturer datasheet terms (`www.vishay.com/doc?91000`) do not grant redistribution rights, the 5 real Vishay PDFs and their verbatim text annotations remain in `build/corpus/` and are evaluated locally rather than in public GitHub Actions runners.
3. **Sub-band geometry selection**: On real multi-MPN or parametric selection tables (`MRS16/MRS25`, `D/CRCW e3`, `VJ Commercial Series`, `150D`), `datasim_pdf` can only extract horizontal sub-bands where a single MPN/column group has non-overlapping value and unit coordinates; full multi-MPN tables fail closed.

### 5.2 Prioritized Recommendations for a Future Parser-Improvement Phase

Based on the measured failure modes in Section 4, future extensions to `datasim_pdf` (requiring a new ADR before implementation) should prioritize:

1. **P1 — Column-Header Unit and Condition Inheritance (`unit_mismatch` / `condition_mismatch` / `ambiguous_numeric`)**:
   - Real capacitor and resistor parametric tables (`vishay_150d_tantalum.pdf`, `vishay_vj_commercial_mlcc.pdf`) frequently declare the unit symbol in the column header (`CAPACITANCE (μF)`, `VOLTAGE (V)`) rather than repeating a dedicated `Unit` column in every row. Supporting an optional explicit header-unit binding in `ElectricalTableLayout` would resolve all 5 `unit_mismatch` / `ambiguous_numeric` failures on `REAL-CASE-07` and all 5 `condition_mismatch` warnings on `REAL-CASE-05`.
2. **P2 — Inline Value+Unit Token Splitting Within Value Cells (`column_assignment_error`)**:
   - In `vishay_mrs16_mrs25.pdf` (`0.4 W`, `200 V`, `75 V`) and `vishay_vj_commercial_mlcc.pdf` (`220 pF`, `10 nF`), value and unit tokens sit inside the same visual column and shift horizontally depending on digit count (`200 V` vs. `75 V`), causing tight `value` and `unit` horizontal spans to overlap across rows by `0.06 pt`. Allowing a value column to split a trailing unit token deterministically would eliminate sub-point horizontal span collisions.
3. **P3 — Hierarchical Group-Row / Merged-Cell Context Propagation (`column_assignment_error`)**:
   - In `vishay_mrs16_mrs25.pdf` (`Insulation voltage:` parent row followed by indented `1 min; U ins` and `continuous` child rows) and `vishay_crcw_e3.pdf` (merged vertical `Operating voltage` and `Resistance range` cells), flat single-row extraction either loses the parent parameter prefix or omits rows whose parameter cell is vertically merged.
4. **P4 — Subscript and Superscript Baseline Coalescing**:
   - Subscripts (`U max.`, `U ins`, `P 70`) and scientific-notation superscripts (`10 -4` in `vishay_mkp1840m_film.pdf`) have shifted `yMin..yMax` bounding boxes in `pdftotext -bbox-layout`, requiring baseline-aware attachment to their host line rather than flat `y`-center clustering alone.

