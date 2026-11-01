# Phase 3 validation: Deterministic component-to-simulation vertical slice

Status: **VERIFIED** on Windows x64 (MSYS2 UCRT GCC 16.2.0, Ninja, Release,
ngspice-47), both in hosted Windows Server UCRT64 GitHub Actions CI and in local clean
Release builds. No real vendor datasheet extraction, nonlinear semiconductor model
verification, or physical device measurement (`MEASURED`) is claimed.

## 1. Initial Phase 3 verification and hosted CI on `main` (`6516a629c75662b83f48704bfd7978bcc299ff78`)

Phase 3 was initially implemented on `feat/phase-3-vertical-slice` (branched from
`origin/main` at `b2f73d5`, where the pre-change baseline passed 4/4 CTest suites:
`unit` 92 checks, `component_model` 270 checks, `backend_errors`, and `ngspice_rc` with
0 skips) and merged into `main` at `6516a629c75662b83f48704bfd7978bcc299ff78`.

Hosted GitHub Actions workflow `RC verification`
([run 41284202716](https://github.com/quocthaile/datasim/actions/runs/41284202716),
job `124782076845` `windows-ucrt`) executed on `main` at commit
`6516a629c75662b83f48704bfd7978bcc299ff78` on 2026-11-01 (`07:17:42Z`–`07:20:30Z`)
and **PASSED** all steps:

- Installed hash-checked `mingw-w64-ucrt-x86_64-ngspice-47-1-any.pkg.tar.zst` and
  verified `ngspice-47`.
- Passed `python -m unittest discover -s tests/ci -v` (5/5 runtime path tests) and
  idempotent `configure_ngspice.py` code-model initialization checks.
- Built clean Release checkout with Ninja (`-DDATASIM_NGSPICE=.../ngspice_con.exe`).
- Passed all CTest `unit` suites (`unit`: 92 checks, `component_model`: 270 checks,
  `vertical_slice`: 1392 checks, `backend_errors`) and `integration` suite
  (`ngspice_rc` real `ngspice-47` execution) with **0 failures and 0 skips**.
- Passed the six-run convergence and repeatability study (`rc_study.exe`).

Before modifying source files for Phase 3 hardening, a local baseline build on
`fix/phase3-validation-hardening` at `6516a629c75662b83f48704bfd7978bcc299ff78`
(`build/phase3h-baseline`) reproduced the exact same 5/5 CTest results (`unit`: 92,
`component_model`: 270, `vertical_slice`: 1392, `backend_errors`, `ngspice_rc`: 640
samples, native `max_error_V = 1.42982e-5`, common-grid `max_error_V = 6.31106e-5`,
0 skips).

## 2. Phase 3 architecture and validation hardening (`fix/phase3-validation-hardening`)

Target `vertical_slice_tests` (CTest name `vertical_slice`, label `unit`) now executes
**1483 deterministic checks** across six suites in
`tests/unit/vertical_slice_tests.cpp`, and `integration_tests` (CTest name
`ngspice_rc`, label `integration`) executes real `ngspice-47` comparison between
knowledge-driven `execute_vertical_rc_run` and manual `execute_rc_run`:

| Work package / suite | Verified positive and negative contracts |
| --- | --- |
| WP1: Circuit assembly | Valid multi-bundle `CircuitAssembly` referencing `SYN-TF0603-1K00-LF` and `SYN-C0G0805-1U0-J50NT` while preserving single-component `ComponentDomainBundle` validity, exact MPNs/suffixes (`LF`, `J50NT`), and 1-based terminal-to-pin-to-node bindings; acceptance of symmetric 2-terminal reversal; rejection of empty/invalid IDs, unsupported topology, non-canonical nodes, duplicate bundles/instances, dangling `ComponentId` / `ComponentKnowledgeId` / `ModelArtifactId`, knowledge/model version mismatches, unapproved knowledge, unresolved conflicts, family/role mismatches, wrong pin/terminal bindings, shorted terminals, and miswired nodes. |
| WP2: Parameter resolution | Resolution of approved typical `dc_resistance = 1000.0 ohm` and `capacitance = 1.0e-6 F` preserving `ObservationId`, `EvidenceLocation`, `SourceDocument`, original notation (`"1.000"`, `"kohm"`, `"uF"`), `conversion_rule`, and `OperatingCondition` records; rejection of missing or fixed-literal model parameter mappings, unsupported extra model parameters (`L_PARASITIC`), duplicate conflicting mappings, conflicting duplicate typical normalized parameters, `minimum`/`maximum` qualifiers, non-electrical categories, non-SI units (`kohm` on normalized parameter), cross-dimension units (`F` on resistor), and runtime scaling expressions in `ModelParameterMapping`. |
| WP3: Model/backend compatibility & metadata policy gate | Rejection of `ModelFormat::spice_subcircuit` (`model.synthetic.capacitor.c0g-subckt`) with both `unsupported_model_format` and `external_artifact_unavailable_in_rc_backend`; rejection of `spice_model_card`, `lookup_table`, and `vendor_specific_artifact`; rejection of unsupported analyses (`dc_operating_point`, `dc_sweep`, `ac_small_signal`, `noise`, `thermal`) and models omitting `transient`; rejection of `unvalidated`, empty `validation_record_refs`, dangling `ModelValidationId`, non-human `ModelValidationRecord` reviewers (`llm_assistant`, `automatic_parser`, `automated_check`), missing validation `evidence_refs`, `failed_validation`, and version-mismatched `out_of_date` (`1.1.0` vs `1.0.0`) models; rejection of synthetic fixtures under `ExecutionPolicy::engineering_approved_only` and acceptance with `ExecutionClassification::test_only_synthetic` under `ExecutionPolicy::test_only_synthetic`; acceptance of metadata-classified non-synthetic policy-gate fixture (`make_non_synthetic_policy_gate_rc_assembly`) under `engineering_approved_only` and rejection of mixed synthetic/non-synthetic component, `SourceDocument`, `ExtractionMethod::synthetic_fixture` observation, or model metadata. |
| WP3: Fail-closed `ModelValidityEnvelope` & operating conditions | Rejection of missing runtime `ambient_temperature_degC`, missing `stimulus_voltage_V` / `initial_voltage_V`, and missing explicit `signal_frequency` in `additional_conditions`; rejection of unsupported envelope quantities (`relative_humidity`, and `continuous_power` on `shunt_capacitor`); rejection of missing (`std::nullopt`) or non-finite (`NaN`, `+Inf`, `-Inf`) numeric values on runtime or parameter operating conditions; rejection of unit mismatches (`K` vs `degC`, `kHz` vs `Hz`, empty units); rejection of values below minimum or above maximum for `ambient_temperature` (`-60 degC`, `150 degC`), `terminal_dc_voltage` (`-60 V`, `60 V`), `continuous_power` ($10^2 / 1000 = 0.100\text{ W} > 0.050\text{ W}$ and bipolar $-5\text{ V} \to +5\text{ V}$ step swing), and `signal_frequency` (`-1 Hz`, `5e7 Hz`); verification of inclusive vs exclusive boundary values (`[-55, 125]` vs `(-55, 125)` degC and `[0, 1e6]` vs `(0, 1e6)` Hz); acceptance of consistent duplicate conditions and rejection of conflicting duplicates across `additional_conditions`, primary `OperatingEnvironment` fields, and `resolved_param.operating_conditions`. |
| WP4 & WP6: Simulation adapter & manual `RcConfig` equivalence | Separation of `RcPulseStimulus`, `RcTransientControls`, and `RcSolverControls` from component parameters; rejection of unsupported solver overrides (`method = "gear"`, `temp = 50 degC`) and invalid RC timing (`step_s = 1e-3`); exact field, `config_json`, `rc_netlist`, netlist FNV-1a-64 hash (`fce4e70faf4e32d4`), 4001-point common grid, `analytical_voltage`, `ramp_voltage`, and `0.05 V` tolerance equality against manual `RcConfig{}`. |
| WP5: Provenance & real `ngspice-47` equivalence | Complete component, knowledge, model, `NormalizedParameterId`, `ObservationId`, `EvidenceId`, `SourceDocumentId`, `conversion_rule`, `OperatingCondition`, `TerminalNodeBinding`, `ExecutionClassification`, and pre-execution FNV-1a-64 content fingerprint recording in `VerticalSliceProvenanceManifest` and `provenance_metadata.json`; verification that lineage mutations (`circuit_id`, `conversion_rule`, `model_version`) alter `provenance_content_id` while post-execution runtime status fields do not; fail-fast pre-execution input gate creating no directory on invalid input; real `ngspice-47` execution confirming byte-identical `circuit.cir`, `results.csv` (SHA-256 `9c923c4d86a96bdf42fe3bbd84a00cc6ae8519f5ac8ea3858b614c30260f26f9`), and `errors.csv` between `execute_vertical_rc_run` and manual `execute_rc_run`. |

## 3. Local clean Release build and Phase 1.5 non-regression results (`build/phase3-hardening`)

Executed on Windows x64 (MSYS2 UCRT GCC 16.2.0 Rev3, CMake 4.4.2, Ninja, Release,
`ngspice-47`):

| CTest suite | Label | Result | Evidence |
| --- | --- | --- | --- |
| `unit` | `unit` | Passed (0.02 s) | `PASS: 92 unit checks` (unchanged Phase 1/1.5 unit checks). |
| `component_model` | `unit` | Passed (0.02 s) | `PASS: 270 component domain checks` (unchanged Phase 2 domain checks). |
| `vertical_slice` | `unit` | Passed (0.03 s) | `PASS: 1483 vertical slice checks` (Phase 3 WP1–WP6 + validation hardening checks). |
| `backend_errors` | `unit` | Passed (1.30 s) | Unchanged subprocess fault, timeout, concurrency, argv, and sentinel checks. |
| `ngspice_rc` | `integration` | Passed (0.32 s) | Real `ngspice-47` manual + vertical slice runs: 640 samples, native `max_error_V = 1.42982e-5`, common-grid `max_error_V = 6.31106e-5`, `rmse_V = 1.13392e-5`, byte-identical `results.csv`/`errors.csv`/`circuit.cir`, `provenance_metadata.json` verified, invalid netlist rejected with exit 1. |

Aggregate local CTest: **5/5 passed, 0 failed, 0 skipped**.

Additional Phase 1.5 non-regression checks:

- `python -m unittest discover -s tests/ci -v`: **5/5 passed** (`OK`).
- `./build/phase3-hardening/rc_study.exe D:/projects/datasim/build/tools/msys2-ngspice/ucrt64/bin/ngspice_con.exe build/phase3-hardening-study`:
  all six runs (10 us, 5 us, 2.5 us, 1.25 us refinements plus two 10 us repeats)
  passed with unchanged common-grid metrics (`6.31106e-5 V`, `1.78925e-5 V`,
  `6.00291e-6 V`, `2.66873e-6 V`), `0 V` repeat delta, and baseline `run-0/results.csv`
  SHA-256 `9c923c4d86a96bdf42fe3bbd84a00cc6ae8519f5ac8ea3858b614c30260f26f9`,
  matching Phase 1.5, Phase 2, and the Phase 3 vertical-slice `results.csv` byte for
  byte.

## 4. Limitations and status boundaries

- **Platform coverage (`VERIFIED` vs unverified)**: Windows x64 (MSYS2 UCRT GCC
  16.2.0 / Ninja / ngspice-47) is **VERIFIED** locally and in hosted Windows Server
  UCRT64 GitHub Actions CI ([run 41284202716](https://github.com/quocthaile/datasim/actions/runs/41284202716)
  on `main` at `6516a629c75662b83f48704bfd7978bcc299ff78`). Linux, macOS, and MSVC
  builds remain unverified.
- **Model and analysis scope (`IMPLEMENTED` / `VERIFIED` vs `PROPOSED`)**: Only ideal
  first-order RC transient simulation (`ModelFormat::analytical_equation`,
  `AnalysisType::transient`) is implemented and verified. External SPICE subcircuit
  loading, nonlinear semiconductor simulation, PDF/OCR ingestion, persistent storage,
  automatic unit conversion, LLM integration, and GUI visualization remain `PROPOSED`.
- **Trust and physical validation (`VERIFIED` vs `MEASURED`)**: Policy-gate unit tests
  verify in-memory metadata and review-record rules only; they do not authenticate
  real reviewer identity, manufacturer identity, or datasheet authenticity, and no
  real manufacturer datasheet ingestion or physical hardware measurement (`MEASURED`)
  has been performed.
