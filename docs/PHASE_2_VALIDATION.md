# Phase 2 validation

Status: **VERIFIED** locally on Windows x64 (MSYS2 UCRT GCC 16.2.0, CMake
4.4.0-rc2, Ninja, Release, ngspice-47). All Phase 2 tests use synthetic passive
fixtures marked `is_synthetic = true`; no real vendor datasheet extraction,
semiconductor model verification, or physical device measurement (`MEASURED`) is
claimed.

## Pre-change baseline check (2026-10-31)

Before modifying any file, `feat/phase-2-component-model` was created from
`origin/main` at `86b924b4b49d69c70c1688a8748cb1c2893adc34`, configured in
`build/phase2-baseline`, built, and tested via CTest:

- `unit`: Passed (`PASS: 92 unit checks`, 0.02 s)
- `backend_errors`: Passed (1.35 s)
- `ngspice_rc`: Passed (`ngspice-47`, 640 samples, `max_error_V = 1.42982e-5`,
  `rmse_V = 8.487e-6`, invalid netlist rejected with exit 1, 0.25 s)
- Aggregate: **3/3 passed, 0 failed, 0 skipped**.

## Phase 2 domain unit test verification

Target `component_tests` (CTest name `component_model`, label `unit`) links only
`datasim_component` and executes **270 deterministic checks** across nine suites in
`tests/unit/component_domain_tests.cpp`:

| Suite | Verified positive and negative contracts |
| --- | --- |
| Synthetic fixtures baseline | End-to-end `validate_domain_bundle` acceptance and `is_synthetic` flags on both `SYN-TF0603-1K00-LF` (thin-film resistor) and `SYN-C0G0805-1U0-J50NT` (C0G ceramic capacitor) bundles. |
| Component identity | Valid identities; exact preservation of manufacturer, part number, and suffixes (`FR-071KL/NOPB`, `LF`, `J50NT`); distinct manufacturers with identical MPNs; rejection of empty/blank/control-character manufacturer, part number, ID, or blank optional suffix. |
| Source document & evidence | Valid document metadata and 0-based `pdf_page_index` independent of Roman/alphanumeric `printed_page_label` (`0` vs `"iii"`, `4` vs `"A-1"`); table and figure locators; rejection of missing `source_document_id`, empty locators, dangling `SourceDocumentId` / `EvidenceId` references, invalid SHA-256 digests, and invalid ISO-8601 dates (`2026-02-30`). |
| Parameter observations | Distinct `minimum`, `typical`, `maximum`, and `unspecified` qualifiers; missing values preserved as `std::nullopt` (never `0.0`); rejection of numeric value without `original_value_text`; verbatim preservation of `<= 0.0015`, `+/-1%`, `10 to 100`, `> 10^10`; rejection of `NaN` and `+/-Inf`; mandatory units on numeric observations and operating conditions; contradictory `unitless` flags; explicit rating categories never inferred from `parameter_name`. |
| Engineering review lifecycle | Exhaustive 5x5 transition matrix check confirming only 4 valid transitions (`candidate -> needs_review`, `needs_review -> approved`, `needs_review -> rejected`, `approved -> superseded`) and rejecting all 21 invalid pairs without mutating the observation; rejection of missing reviewer, missing rationale, approval without evidence, non-human approval (`llm_assistant`, `automatic_parser`, `automated_check`), self-approval matching `extraction_tool`, blank-value approval, missing superseding ID, and self-supersession. |
| Component knowledge | Approved observation promotion; exclusion of `rejected`, `candidate`, `needs_review`, and `superseded` observations; detection of undocumented competing approved observations; visible `unresolved` conflict storage blocking promotion; `resolved_by_review` human conflict resolution promoting only the selected observation while blocking silent overwrite; rejection of reversed `min/typ/max` bounds, altered original notation, reclassified `absolute_maximum_rating`, broken `version_history` chains, and synthetic-to-non-synthetic provenance leaks. |
| Model artifact & independent validation | Valid analytical equation and SPICE subcircuit descriptors; rejection of `< 2` terminals, reversed/non-contiguous terminal positions, duplicate terminal/pin names, malformed versions, missing vendor/license/URI metadata, empty/inverted/open-point/unbounded/non-finite validity envelopes, and `absolute_maximum_rating` bound as a behavioral model parameter; verification that model validation status remains `unvalidated` despite approved knowledge, transitions to `validated_within_envelope` only with a human `ModelValidationRecord`, becomes `out_of_date` on model version bump, and reports `failed_validation` without altering knowledge approval. |
| Parameter unit consistency | Exact unit match (`ohm` vs `ohm`, `F` vs `F`); unitless tokens (`1`, `unitless`, `dimensionless`, `-`); differing units in same physical dimension with explicit conversion contract (`kohm` vs `ohm` with `mapping_expression`); rejection of differing units without conversion contract (no automatic conversion); rejection of incompatible dimensions (`F` vs `ohm`, `V` vs `A`); rejection of unitless mapped to dimensional; bundle-level parameter unit validation rejection. |
| Source document component association | Exact manufacturer part number matching; multi-part family document matching containing exact MPN; rejection of shared prefixes without suffix; rejection of truncated suffixes; rejection of family documents missing exact MPN; detection and rejection of duplicate entries in `associated_part_numbers`; bundle-level document applicability rejection. |

## Post-implementation CTest and Phase 1.5 non-regression results

Executed in clean build directory `build/phase2-wp3`:

| CTest suite | Label | Result | Evidence |
| --- | --- | --- | --- |
| `unit` | `unit` | Passed (0.02 s) | `PASS: 92 unit checks` (unchanged Phase 1/1.5 analytical, CSV, grid, JSON checks). |
| `component_model` | `unit` | Passed (0.02 s) | `PASS: 270 component domain checks` (Phase 2 domain model, synthetic fixtures, and validation hardening). |
| `backend_errors` | `unit` | Passed (1.32 s) | Subprocess faults, timeout kill, concurrent workspaces, literal argv, input gate, sentinel preservation. |
| `ngspice_rc` | `integration` | Passed (0.20 s) | Real `ngspice-47` execution, 640 samples, `max_error_V = 1.42982e-5`, `rmse_V = 8.487e-6`, common-grid 4001-point report, invalid netlist exit 1. |

Aggregate local CTest: **4/4 passed, 0 failed, 0 skipped**.

Additional Phase 1.5 non-regression checks run after Phase 2 implementation:

- `python -m unittest discover -s tests/ci -v`: **5/5 passed** (`OK`).
- `./build/phase2-wp3/rc_study.exe D:/projects/datasim/build/tools/msys2-ngspice/ucrt64/bin/ngspice_con.exe build/phase2-rc-study-check`:
  all six runs (10 us, 5 us, 2.5 us, 1.25 us refinements plus two 10 us repeats)
  passed with unchanged common-grid metrics (`6.31106e-5 V`, `1.78925e-5 V`,
  `6.00291e-6 V`, `2.66873e-6 V`) and baseline `run-0/results.csv` SHA-256
  `9c923c4d86a96bdf42fe3bbd84a00cc6ae8519f5ac8ea3858b614c30260f26f9`,
  matching the Phase 1.5 baseline byte for byte.

## Limitations

- Only Windows x64 (MSYS2 UCRT GCC 16.2.0 / Ninja / ngspice-47) and hosted Windows
  Server UCRT64 CI are verified; Linux, macOS, and MSVC remain unverified.
- Synthetic Resistor and Capacitor fixtures verify domain contract structure and
  referential rules only; they are not real manufacturer data and do not validate
  any physical device or simulator model accuracy.
