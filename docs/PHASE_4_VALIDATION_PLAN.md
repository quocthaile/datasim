# Phase 4 validation plan: Evidence intake (Phase 4A planning)

Status: **PROPOSED test design.** No test in this document exists or has run. No
benchmark result, extraction accuracy, or real-datasheet outcome is claimed. Context:
[Phase 4 plan](PHASE_4_PLAN.md), [ADR-0005](decisions/ADR-0005-evidence-intake-boundary.md).
All type and function names prefixed `PROPOSED` below do not exist yet; names without
that marker are existing APIs.

## 1. Principles

- Fixtures are **synthetic, legally redistributable, and fictional**: fictitious
  manufacturer and part numbers (for example `SYN-…` prefixes as in
  `tests/unit/component_fixtures.cpp`), `is_synthetic = true`,
  `ExtractionMethod::synthetic_fixture` or `manual_entry` marked as synthetic by origin
  class, `urn:datasim:synthetic:…` URLs, placeholder `content_sha256` that is never
  presented as authentic. No vendor datasheet text, table, curve, or model is copied.
- Every negative test asserts a specific `ValidationRuleCode` (existing),
  `IntakeDiagnosticCode` (`PROPOSED`), or the documented `std::invalid_argument` and that **no candidate bundle** is produced.
- Tests are deterministic, filesystem- and network-free, and add no dependency.
- Existing suites stay unchanged and green: `unit` (92), `component_model` (270),
  `vertical_slice` (1483), `backend_errors`, `ngspice_rc`, `rc_study`.
- A skipped integration test is `NOT VERIFIED`, not a pass.

## 2. Test matrix

Columns: **ID**, scenario, input defect, expected result, mechanism. `D` = existing
domain rule; `I` = `PROPOSED` intake rule.

### 2.1 Source, identity, and references

| ID | Scenario | Expected | Mech. |
| --- | --- | --- | --- |
| P-01 | Valid synthetic manifest: document, two evidence locations, five candidates (min/typ/max, AMR, ROC) | bundle valid; all `ReviewState::candidate`; `knowledge` empty | D+I |
| P-02 | `pdf_page_index=2` with `printed_page_label="1"` | both preserved verbatim, neither derived | D |
| P-03 | Observation with `evidence_refs` to two locations | links preserved in order | D |
| N-01 | Observation references unknown `EvidenceId` | `unresolved_reference` | D |
| N-02 | `EvidenceLocation::source_document_id` unknown | `unresolved_reference` | D |
| N-03 | Duplicate `ObservationId` / `EvidenceId` / `SourceDocumentId` | `duplicate_identifier` | D |
| N-04 | Document `associated_part_numbers` lacks component `part_number` | `incompatible_source_document_part_number` | D |
| N-05 | Same base part number, different `part_number_suffix` than document's applicable number | `PROPOSED` `part_number_suffix_mismatch` (domain accepts today, finding F2) | I |
| N-06 | Manufacturer ≠ `SourceDocument::publisher` without declaration | `PROPOSED` `manufacturer_publisher_mismatch` (F2) | I |
| N-07 | Case-different part number (`abc123` vs `ABC123`) | rejected; no case folding | D |
| N-08 | `pdf_page_index` ≥ `declared_pdf_page_count` | `PROPOSED` `page_index_out_of_range` (F3) | I |
| N-09 | `pdf_page_index` and `printed_page_label` both absent and no other locator | `empty_evidence_locator` | D |
| N-10 | Printed label supplied but PDF index silently set equal to label | `PROPOSED` `derived_page_index` (page index must be explicit or absent) | I |
| N-11 | Observation `component_id` ≠ manifest component | `unresolved_reference` | D |

### 2.2 Values, units, qualifiers, categories

| ID | Scenario | Expected | Mech. |
| --- | --- | --- | --- |
| N-12 | `parsed_numeric_value` present, `original_value_text` absent | `missing_numeric_source_text` | D |
| N-13 | `parsed_numeric_value` present, no `original_unit_symbol`, `unitless=false` | `missing_required_unit` | D |
| N-14 | `unitless=true` and `original_unit_symbol` set | `contradictory_unitless_flag` | D |
| N-15 | Value cell missing | candidate kept with `std::nullopt`; attempt to approve → `missing_value_cannot_be_approved` | D |
| N-16 | Raw text `"1,000"`, `"1–5"`, `"±5%"`, `"<0.1"`, `"1.0(1)"`, `"5 typ."` | `parsed_numeric_value == std::nullopt` plus `PROPOSED` `ambiguous_numeric_text` | I |
| N-17 | `NaN`/`Inf` parsed value | `non_finite_numeric_value` | D |
| P-04 | Raw `"1.000"` unit `"kohm"` | `parsed_numeric_value == 1.0` in **original** unit; no ×1000 | I |
| N-18 | Intake asked to emit SI value / convert `kohm`→`ohm`, `mV`→`V`, `°F`→`degC` | rejected `PROPOSED` `unsupported_unit_conversion`; normalization is only via reviewed `NormalizedParameter` | I |
| N-19 | Same row, min and max cells combined into one observation | rejected `PROPOSED` `merged_qualifier_cells`; separate observations required | I |
| P-05 | Three observations (min/typ/max) of one parameter | three distinct `ObservationId`; `ValueQualifier` each set | D+I |
| N-20 | Qualifier text says `Max`, `ValueQualifier::minimum` supplied | `PROPOSED` `qualifier_text_mismatch` | I |
| N-21 | Reversed bounds (min value > max value for same parameter and conditions) | `reversed_qualifier_bounds` | D |
| N-22 | Section titled "Absolute maximum ratings", category supplied as `recommended_operating_condition` or `electrical_characteristic` | `PROPOSED` `category_section_mismatch`; never auto-corrected | I |
| N-23 | AMR candidate later offered as model parameter mapping | `absolute_maximum_misused_as_model_parameter` / `resolve_rc_parameters` category rejection | D (Phase 3) |

### 2.3 Operating conditions, footnotes, ambiguity

| ID | Scenario | Expected | Mech. |
| --- | --- | --- | --- |
| P-06 | Temperature `"25"` `"degC"`, voltage `"5"` `"V"`, frequency `"1"` `"kHz"` conditions with `numeric_value` | preserved verbatim (`kHz` not converted to `Hz`) | D+I |
| N-24 | Condition without unit (`""`) | `validate_operating_condition` issue | D |
| N-25 | Condition text `"T_A = 25 to 85"` | `numeric_value == std::nullopt` + `ambiguous_numeric_text`, text preserved | I |
| N-26 | Same parameter/qualifier at two temperatures | two observations, each with its own conditions; not a conflict | I |
| N-27 | Footnote marker on value, footnote not captured | `PROPOSED` `unresolved_footnote_marker`; value stays unparsed | I |
| N-28 | Footnote text recorded in `footnote_refs` | preserved; value still requires review | D |
| N-29 | Ambiguous table cell (merged cells, "typ/max" sharing) | candidate with `ambiguity_notes`, `parsed_numeric_value == std::nullopt` | I |

### 2.4 Conflicts

| ID | Scenario | Expected | Mech. |
| --- | --- | --- | --- |
| N-30 | Two candidates, same name/qualifier/category/conditions, different values | both kept; `PROPOSED` `conflict_candidate` diagnostic; no averaging or dropping | I |
| N-31 | Promotion of conflicted observations without declared `ParameterConflict` | `undeclared_parameter_conflict` / `conflicted_observation_promoted` | D |
| N-32 | Same value from two different locations | both preserved with their own `evidence_refs` (no dedup) | I |

### 2.5 Review and trust boundary

| ID | Scenario | Expected | Mech. |
| --- | --- | --- | --- |
| N-33 | Intake output contains non-`candidate` state or non-empty `review_history` | impossible by construction; any hand-injected one rejected `PROPOSED` `non_candidate_state_in_intake` | I |
| N-34 | `candidate → approved` directly | `invalid_review_transition` | D |
| N-35 | Approval by `ActorKind::llm_assistant` / `automatic_parser` / `automated_check` | `unauthorized_reviewer_kind` | D |
| N-36 | `candidate → needs_review` by `llm_assistant` or `automatic_parser` | `unauthorized_reviewer_kind`; `automated_check` and `human_engineer` accepted | D |
| N-37 | Approval with empty rationale, bad timestamp, or unresolved `evidence_refs` | rejected | D |
| N-38 | `extraction_tool == reviewer.id` | `self_approval_prohibited` | D |
| N-39 | Curator id equals approving human id (D5) | currently accepted by domain; regression test documents behavior; policy test only if D5 adopts four-eyes | D / I |
| N-40 | Attempt `promote_parameter_to_knowledge` with candidate/`needs_review`/`rejected` observation | throws `std::invalid_argument` ("only approved observations may enter ComponentKnowledge"); `validate_component_knowledge` reports `unapproved_observation_in_knowledge` if such a record is injected | D |
| N-41 | Attempt `make_normalized_parameter` from non-approved observation | throws `std::invalid_argument` | D |
| N-42 | Reviewer metadata asserts `human_engineer` with no real identity | accepted by design; test and docs state this is metadata, not authentication | D |

### 2.6 Synthetic versus real provenance

| ID | Scenario | Expected | Mech. |
| --- | --- | --- | --- |
| N-43 | Component `is_synthetic=false`, document `is_synthetic=true` | `synthetic_provenance_mismatch` | D |
| N-44 | Component synthetic, document non-synthetic or observation `manual_entry` without synthetic origin class | `PROPOSED` `inconsistent_synthetic_markers` | I |
| N-45 | `IntakeOriginClass::synthetic_fixture` with a plausible real publisher name | rejected unless fictitious-marker rule satisfied (`PROPOSED` `synthetic_masquerading_as_manufacturer`) | I |
| N-46 | Any attempt to set a "manufacturer-verified" or "authenticated" origin | no such enumerator exists; compile-time impossibility documented | I |
| N-47 | `redistribution_permitted=false` document with embedded content or excerpt in fixture notes | rejected `PROPOSED` `rights_violation_embedded_content` | I |
| N-48 | Placeholder `content_sha256` | accepted syntactically; documentation/test assert it is never treated as authenticity | D |

### 2.7 Rejection before simulation

| ID | Scenario | Expected | Mech. |
| --- | --- | --- | --- |
| N-49 | Candidate-only bundle passed to `CircuitAssembly` / `resolve_rc_parameters` | `unapproved_component_knowledge` / missing knowledge; no workspace directory created | D (Phase 3) |
| N-50 | Approved knowledge from synthetic intake run under `ExecutionPolicy::engineering_approved_only` | rejected; `test_only_synthetic` classification only | D (Phase 3) |
| N-51 | Approved observation lacking a validated model | `unvalidated` rejection | D (Phase 3) |
| N-52 | Non-SI unit (`kohm`) on `NormalizedParameter` supplied to resolution | rejected (Phase 3 SI gate) | D (Phase 3) |
| P-07 | End-to-end synthetic: intake → human review → normalized → knowledge → model → ideal-RC run; byte-identical to manual `RcConfig` | pass only with real ngspice; otherwise `NOT VERIFIED` | D (Phase 3) + I |

## 3. Evaluation metrics (definitions only)

Metrics apply to a future labeled corpus; each is computed and reported
**separately** and never aggregated into a single score. No values exist or are claimed.
Ground truth is engineer-reviewed labels against the original document; unlabeled items
are excluded, not counted as correct.

| Metric | Definition |
| --- | --- |
| Extracted value correctness | Fraction of labeled observations whose `parsed_numeric_value` equals the label within exact equality for decimals as written (no tolerance); items intentionally left `std::nullopt` are scored in a separate "abstention" column; precision of non-null values and recall of labeled-parseable values reported separately. |
| Unit correctness | Exact string match of `original_unit_symbol` (and `unitless`) with the label; no equivalence classes. |
| Qualifier correctness | Exact `ValueQualifier` match; confusion matrix over min/typ/max/unspecified reported. |
| Operating-condition correctness | Per-condition match of `parameter_name`, `original_value_text`, `unit_symbol`; set-level precision/recall of conditions per observation; missing and extra conditions counted separately. |
| Provenance correctness | Per observation: correct `SourceDocumentId`, correct `pdf_page_index`, correct `printed_page_label`, correct table/section/cell where labeled, correct footnote set; reported as separate sub-metrics. |
| Safety counters | Count of candidates entering non-`candidate` state during intake (must be 0), count of AMR candidates re-categorized (must be 0), count of silent unit conversions (must be 0). |

Sampling, corpus selection, and acceptance thresholds require owner approval and a
separately licensed corpus; synthetic fixtures establish only that the checks behave as
specified, not extraction accuracy.

## 4. Acceptance criteria (for the later Phase 4B implementation)

1. All P- and N- cases above pass with each negative case asserting its specific code
   and the absence of a candidate bundle.
2. Existing suites: `unit` 92, `component_model` 270, `vertical_slice` 1483 checks and
   `backend_errors` pass unchanged; `ngspice_rc` and `rc_study` pass with real
   `ngspice-47` in a run that records "0 skipped", with `results.csv` SHA-256
   `9c923c4d86a96bdf42fe3bbd84a00cc6ae8519f5ac8ea3858b614c30260f26f9` unchanged and the
   0.05 V tolerance intact.
3. `git diff` shows no change to `src/datasim/simulation`, `src/datasim/component`,
   or `src/datasim/vertical` (unless D2 authorizes a separate hardening PR).
4. No vendor datasheet, model file, or excerpt is added; all fixtures are marked
   synthetic and none is labeled `IMPLEMENTED`/`VERIFIED` as real-manufacturer data.
5. No new dependency; no persistence, serialization, GUI, or LLM code.
6. Documentation status terms (`PROPOSED`, `PLANNED`, `IMPLEMENTED`, `VERIFIED`,
   `MEASURED`) match the [charter](PROJECT_CHARTER.md); links resolve.
7. Hosted Windows CI passes; local runs without ngspice are reported as `NOT VERIFIED`
   for integration.

## 5. Documentation checks for this Phase 4A branch

Relative links resolve, fenced blocks balanced, Mermaid parses, `PROPOSED` markers
present on all new names, diff limited to documentation. Results are recorded in the
final report, not here.
