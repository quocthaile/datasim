# Phase 4E Corpus Protocol — Eligibility, Governance, Annotation Schema, and Metrics

- **Phase**: 4E
- **Status**: IMPLEMENTED and VERIFIED on `feat/phase-4e-datasheet-evaluation-corpus`
- **Schema Versions**:
  - Corpus Manifest: `datasim.phase4e.corpus.v1`
  - Ground-Truth Annotations: `datasim.phase4e.annotations.v1`
  - Evaluation Report: `datasim.phase4e.eval_report.v1`

## 1. Supported Extraction Subset and Document Layout Categories

The Phase 4C production parser (`datasim::pdf::extract_electrical_table(const ExtractionRequest&)`) is a bounded, deterministic single-page table adapter built on Poppler `pdfinfo` and `pdftotext -bbox-layout`. It requires an explicitly configured `ElectricalTableLayout` (`region_x_min..max`, `region_y_min..max`, `row_y_tolerance`, `header_y_min..max`, and mutually non-overlapping horizontal spans for `parameter_column`, `unit_column`, `minimum_column`, `typical_column`, `maximum_column`, and optional `condition_column`).

Every candidate document and table case in the Phase 4E evaluation corpus must be assigned to one of six explicit categories:

| Category ID | Category Description | Expected Parser Behavior |
| --- | --- | --- |
| `supported_single_page_table` | **1. Supported text-selectable single-page electrical tables**: horizontal rows within `[region_x_min, region_x_max] x [region_y_min, region_y_max]`, single-line row alignment within `row_y_tolerance`, dedicated non-overlapping `parameter`, `unit`, and `min`/`typ`/`max` columns, and unambiguous scalar values. | Emits validated `CandidateBundle` (`review_state = candidate`) with parsed scalar values in original units. |
| `partially_supported_ambiguous` | **2. Partially supported layouts with ambiguities**: geometry fits non-overlapping column spans so candidate rows are extracted, but cells contain locale-ambiguous numbers (`1,000`), footnotes (`1.0*`), blank row unit cells (unit declared only in column header), hierarchical row sub-labels (`1 min; U ins`), or condition cells lacking inline `PARAMETER=value UNIT` syntax. | Emits `CandidateBundle` while leaving ambiguous/unitless scalars as `std::nullopt`, preserving `raw_conditions_text`, and recording explicit extraction/intake diagnostics. |
| `unsupported_complex_layout` | **3. Unsupported complex layouts**: multi-MPN side-by-side columns with shared/spanned rows, merged vertical cells, inline value+unit cells with horizontal overlap across rows, scientific-notation superscripts on separate `y` baselines, or multi-axis frequency/voltage matrices. | **Fails closed** (`candidate_bundle = std::nullopt`) with deterministic diagnostics. Not classified as a parser defect when it fails closed as designed. |
| `image_only_or_scanned` | **4. Image-only or scanned content**: PDF pages containing raster XObjects or vector outlines without selectable text words. | **Fails closed** (`selected page has no selectable text; image-only pages are unsupported`). |
| `missing_or_inaccessible_document` | **5. Missing or inaccessible documents**: official manufacturer URL cannot be retrieved under strict TLS verification or local PDF is absent. | Excluded before extraction and counted under `rights_or_document_unavailable`. |
| `rights_unestablished` | **6. Documents whose evaluation rights cannot be established**: source provenance or usage terms cannot be verified. | Excluded from evaluation and counted under `rights_or_document_unavailable`. |

## 2. Pilot Corpus Recruitment and Eligibility Assessment

Phase 4E targeted two real resistor datasheets, two real capacitor datasheets, and two complex/negative control cases. Seven real-world candidate documents were inspected on November 5, 2026:

| Document ID | Manufacturer | MPN / Family | Doc No. / Rev | Source URL | Eligibility Status | Inclusion / Exclusion Reason |
| --- | --- | --- | --- | --- | --- | --- |
| `doc.vishay.mrs16_mrs25` | Vishay BCcomponents | `MRS16`, `MRS25` (Thin Film Leaded Resistors) | `28724` / `07-Mar-16` | `https://www.vishay.com/docs/28724/mrs16m25.pdf` | `eligible` (Local `build/corpus/`) | Official manufacturer PDF retrieved over verified TLS; SHA-256 `645bb41aaa34f2a72fc19fc71d02ddb14ed400ae97634684ad1745189505f47e` (`4` pages). Contains both a partially supported voltage sub-band (`REAL-CASE-01`) and an unsupported full multi-MPN table (`REAL-CASE-02`). |
| `doc.vishay.crcw_e3` | Vishay | `D10/CRCW0402 e3`, `D11/CRCW0603 e3`, `D12/CRCW0805 e3` (Thick Film Chip Resistors) | `20035` / `14-Apr-2026` | `https://www.vishay.com/docs/20035/dcrcwe3.pdf` | `eligible` (Local `build/corpus/`) | Official manufacturer PDF retrieved over verified TLS; SHA-256 `a10e1cf2bd4f0f6ef85de250077adb463cae243c2e8e0509354e09f050d24a92` (`13` pages). Contains a supported operating-voltage sub-band (`REAL-CASE-03`) and an unsupported merged-cell TCR/resistance-range table (`REAL-CASE-04`). |
| `doc.vishay.vj_commercial_mlcc` | Vishay Vitramon | `VJ0402`, `VJ0603`, `VJ0805`, `VJ1206`, `VJ1210` (`VJ Commercial Series` MLCC) | `45199` / `08-May-2026` | `https://www.vishay.com/docs/45199/vjcommercialseries.pdf` | `eligible` (Local `build/corpus/`) | Official manufacturer PDF retrieved over verified TLS; SHA-256 `6c9b40c81d312ba6dbe9d0a4e687ae18185efeebf8cb98e6d709020efa550211` (`17` pages). Contains a partially supported `C0G (NP0)` minimum-capacitance sub-band (`REAL-CASE-05`) and an unsupported multi-unit `QUICK REFERENCE DATA` full table (`REAL-CASE-06`). |
| `doc.vishay.150d_tantalum` | Vishay Sprague | `150D` (Solid-Electrolyte TANTALEX Capacitors) | `40015` / `26-Mar-2026` | `https://www.vishay.com/docs/40015/150d.pdf` | `eligible` (Local `build/corpus/`) | Official manufacturer PDF retrieved over verified TLS; SHA-256 `197eeaedf55a47633246a565a73c8c078cc7da3a7246a472f0db4a48dbf523a7` (`16` pages). Evaluates header-only unit columns in `STANDARD RATINGS` (`REAL-CASE-07`). |
| `doc.vishay.mkp1840m_film` | Vishay Roederstein | `MKP1840 M` (Metallized Polypropylene Film Capacitors) | `26018` / `27-Apr-2018` | `https://www.vishay.com/docs/26018/mkp1840m.pdf` | `eligible` (Local `build/corpus/`) | Official manufacturer PDF retrieved over verified TLS; SHA-256 `11cdd98ae202acb7dfaf013ff4d98e5b4c4a5abd8e1330dbf8471febe9958fe0` (`13` pages). Serves as a negative/control case for multi-frequency dissipation-factor and superscript `10 -4` matrices (`REAL-CASE-08`). |
| `doc.kemet.r82_film` | KEMET (YAGEO Group) | `R82` (Radial Metallized Polyester Film Capacitors) | `KEM_F3101_R82` | `https://content.kemet.com/datasheets/KEM_F3101_R82.pdf` | `excluded` | `tls_certificate_expired_on_official_source_host`: `content.kemet.com` failed strict X.509 verification (`CERTIFICATE_VERIFY_FAILED: certificate has expired`) on 2026-11-05. Excluded without bypassing TLS verification. |
| `doc.kemet.f862_film` | KEMET (YAGEO Group) | `F862` (Metallized Polypropylene Film X2 Capacitors) | `KEM_F3092_F862` | `https://content.kemet.com/datasheets/KEM_F3092_F862.pdf` | `excluded` | `tls_certificate_expired_on_official_source_host`: `content.kemet.com` failed strict X.509 verification (`CERTIFICATE_VERIFY_FAILED: certificate has expired`) on 2026-11-05. Excluded without bypassing TLS verification. |

## 3. Licensing and Data Governance Rules

1. **No redistribution of manufacturer PDFs**: All downloaded vendor datasheets carry standard copyright protection and the Vishay Legal Disclaimer (`www.vishay.com/doc?91000`). Public download permits local engineering inspection (`evaluation_permitted = true`) but does not grant redistribution rights (`redistribution_permitted = false`).
2. **Untracked local storage**: Real PDFs (`build/corpus/*.pdf`) and verbatim vendor ground-truth annotations (`build/corpus/real_ground_truth.json`) reside exclusively under `build/corpus/`, which is ignored by `.gitignore`.
3. **No CI artifact upload of vendor PDFs**: GitHub Actions workflows never download or upload vendor PDFs. Hosted CI executes the synthetic corpus regression suite (`tests/evaluation/synthetic_corpus_manifest.json` and `tests/evaluation/synthetic_ground_truth.json`) and validates the schema of `evaluation/real_corpus_manifest.json`.
4. **Public repository contents**: Only source/rights assessment metadata, SHA-256 digests, coordinate geometries (`evaluation/real_corpus_manifest.json`), synthetic fixtures (`tests/fixtures/*.pdf`), and aggregate evaluation metrics (`docs/PHASE_4E_VALIDATION.md`) are committed to Git.

## 4. Ground-Truth Annotation Schema (`datasim.phase4e.annotations.v1`)

Ground-truth files must declare:

- `schema_version`: `"datasim.phase4e.annotations.v1"`
- `annotation_set_id`: unique identifier string
- `annotator_count`: integer count of human annotators (`1` in Phase 4E)
- `inter_annotator_agreement_claimed`: boolean (`false`; the evaluator rejects any file where `annotator_count == 1` and `inter_annotator_agreement_claimed != false`)
- `cases`: array of annotated table cases, each specifying:
  - `case_id`, `document_id`, `expected_sha256`, `pdf_page_index` (0-based), `printed_page_label`, `table_id`
  - `annotation_author`, `review_status`, and non-empty `adjudication_history`
  - `observations`: array of expected cell observations, each containing:
    - `entry_id`: unique ground-truth entry ID
    - `row_index`: 1-based physical row index within the selected table band
    - `cell_or_region`: `"row=<row_index>;column=<min|typ|max>"`
    - `parameter_name`: expected parameter or row identifier string
    - `original_value_text`: exact source value string
    - `original_unit_symbol`: expected unit string or `null`
    - `qualifier`: `"minimum"`, `"typical"`, `"maximum"`, or `"unspecified"`
    - `expected_parsed_numeric_value`: expected unambiguous scalar float in original unit, or `null` when ambiguous/unitless
    - `operating_conditions`: array of `{parameter_name, original_value_text, unit_symbol, numeric_value}`
    - `raw_conditions_text`: expected raw condition string or `null`
    - `category`: `ParameterCategory` string
    - `footnote_refs`: array of footnote marker strings (e.g., `["*"]`)
    - `expected_extraction_outcome`: `"candidate_parsed"`, `"candidate_unparsed_ambiguous"`, `"candidate_unparsed_blank_unit"`, or `"rejected_or_unsupported"`
    - `in_supported_subset`: boolean

## 5. Matching Policy and Evaluation Metrics

### 5.1 Deterministic Observation Matching

For every case with `expected_outcome == "extract_candidates"`, expected observations in ground truth ($G$) and candidate observations emitted by `datasim_pdf` ($E$) are keyed by the 5-tuple:

$$\text{key} = (\text{document\_id}, \text{pdf\_page\_index}, \text{table\_id}, \text{row\_index}, \text{qualifier})$$

where `row_index` is parsed from the value evidence location (`cell_or_region = "row=<N>;column=<min|typ|max>"`).

### 5.2 Observation-Level Precision, Recall, and F1

- **True Positive (`TP`)**: A location-matched pair $(g \in G, e \in E)$ where all core observation fields agree (`raw_value`, `original_unit`, `qualifier`, `parameter_label`, `evidence_location`, and `parsed_scalar`).
- **False Positive (`FP`)**: Any extra/duplicate extracted observation at the same key, any extracted observation at an unmatched key (`spurious_observation`), or a location-matched observation whose core extracted fields disagree with ground truth.
- **False Negative (`FN`)**: Any ground-truth observation in $G$ not produced by the parser (`missing_observation`, including when a supported/partially supported table fails closed), or a location-matched observation whose core extracted fields disagree with ground truth.
- **Explicit Denominators**:
  - $\text{Precision} = \frac{\text{TP}}{\text{TP} + \text{FP}}$ (`null` / `undefined` when $\text{TP} + \text{FP} = 0$)
  - $\text{Recall} = \frac{\text{TP}}{\text{TP} + \text{FN}}$ (`null` / `undefined` when $\text{TP} + \text{FN} = 0$)
  - $\text{F1} = \frac{2\,\text{TP}}{2\,\text{TP} + \text{FP} + \text{FN}}$ (`null` / `undefined` when $2\,\text{TP} + \text{FP} + \text{FN} = 0$)

### 5.3 Field-Level Agreement

Across all location-matched pairs $(g, e)$, the evaluator independently reports numerators, denominators, and agreement rates for eight fields:

1. `raw_value` (`original_value_text`)
2. `original_unit` (`original_unit_symbol`)
3. `qualifier` (`minimum` / `typical` / `maximum` / `unspecified`)
4. `parameter_label` (`parameter_name`)
5. `operating_conditions` (structured `operating_conditions` + `raw_conditions_text` sidecar)
6. `evidence_location` (`pdf_page_index`, `printed_page_label`, `table_or_figure_id`, `cell_or_region`)
7. `footnote_refs` (`EvidenceLocation::footnote_refs`)
8. `parsed_scalar` (`parsed_numeric_value`, verifying both exact float equality and `null` preservation on ambiguous or blank-unit cells)

### 5.4 Four-Part Reporting Separation

Reports never combine supported and unsupported cases into a single accuracy percentage. Every report is partitioned into:

- **Section A**: Corpus eligibility and layout coverage.
- **Section B**: Extraction quality conditional on supported / partially supported layouts.
- **Section C**: Fail-closed behavior for unsupported / negative layouts (`expected_fail_closed_cases`, `actual_fail_closed_cases`, `false_accept_cases`, `diagnostic_matched_fail_closed_cases`).
- **Section D**: Reproducibility (`repeat_runs_verified`, `repeat_run_mismatches`), candidate-only boundary verification, and the 16-label error taxonomy.

## 6. Error Taxonomy

Every diagnostic finding or ground-truth discrepancy is mapped to one of 16 standard taxonomy labels while preserving the original `datasim_pdf`, `IntakeDiagnosticCode`, and `ValidationRuleCode` messages verbatim:

1. `unsupported_layout`
2. `missing_selectable_text`
3. `invalid_table_region`
4. `column_assignment_error`
5. `row_segmentation_error`
6. `missing_observation`
7. `spurious_observation`
8. `raw_value_mismatch`
9. `unit_mismatch`
10. `qualifier_mismatch`
11. `condition_mismatch`
12. `evidence_location_mismatch`
13. `ambiguous_numeric`
14. `source_identity_mismatch`
15. `backend_or_environment_failure`
16. `rights_or_document_unavailable`

