# Phase 3 validation: Deterministic component-to-simulation vertical slice

Status: **VERIFIED** locally on Windows x64 (MSYS2 UCRT GCC 16.2.0, CMake
4.4.0-rc2, Ninja, Release, ngspice-47). No real vendor datasheet extraction,
semiconductor model verification, or physical device measurement (`MEASURED`) is
claimed.

## Pre-change baseline check (2026-11-01)

Before modifying any source file, `feat/phase-3-vertical-slice` was created from
`origin/main` at `b2f73d5` (after PR #2 merged Phase 2 into `main`), configured in
`build/phase3-baseline`, built, and tested via CTest:

- `unit`: Passed (`PASS: 92 unit checks`, 0.02 s)
- `component_model`: Passed (`PASS: 270 component domain checks`, 0.02 s)
- `backend_errors`: Passed (1.34 s)
- `ngspice_rc`: Passed (`ngspice-47`, 640 samples, `max_error_V = 1.42982e-5`,
  `rmse_V = 8.487e-6`, invalid netlist rejected with exit 1, 0.19 s)
- Aggregate: **4/4 passed, 0 failed, 0 skipped**.

## Phase 3 unit and integration test verification

Target `vertical_slice_tests` (CTest name `vertical_slice`, label `unit`) executes
**1392 deterministic checks** across six suites in
`tests/unit/vertical_slice_tests.cpp`, and `integration_tests` (CTest name
`ngspice_rc`, label `integration`) executes real `ngspice-47` comparison between
knowledge-driven `execute_vertical_rc_run` and manual `execute_rc_run`:

| Work package / suite | Verified positive and negative contracts |
| --- | --- |
| WP1: Circuit assembly | Valid multi-bundle `CircuitAssembly` referencing `SYN-TF0603-1K00-LF` and `SYN-C0G0805-1U0-J50NT` while preserving single-component `ComponentDomainBundle` validity, exact MPNs/suffixes (`LF`, `J50NT`), and 1-based terminal-to-pin-to-node bindings; acceptance of symmetric 2-terminal reversal; rejection of empty/invalid IDs, unsupported topology, non-canonical nodes, duplicate bundles/instances, dangling `ComponentId` / `ComponentKnowledgeId` / `ModelArtifactId`, knowledge/model version mismatches, unapproved knowledge, unresolved conflicts, family/role mismatches, wrong pin/terminal bindings, shorted terminals, and miswired nodes. |
| WP2: Parameter resolution | Resolution of approved typical `dc_resistance = 1000.0 ohm` and `capacitance = 1.0e-6 F` preserving `ObservationId`, `EvidenceLocation`, `SourceDocument`, original notation (`"1.000"`, `"kohm"`, `"uF"`), `conversion_rule`, and `OperatingCondition` records; rejection of missing or fixed-literal model parameter mappings, unsupported extra model parameters (`L_PARASITIC`), duplicate conflicting mappings, conflicting duplicate typical normalized parameters, `minimum`/`maximum` qualifiers, non-electrical categories, non-SI units (`kohm` on normalized parameter), cross-dimension units (`F` on resistor), and runtime scaling expressions in `ModelParameterMapping`. |
| WP3: Model/backend compatibility & synthetic policy | Rejection of `ModelFormat::spice_subcircuit` (`model.synthetic.capacitor.c0g-subckt`) with both `unsupported_model_format` and `external_artifact_unavailable_in_rc_backend` (never assuming external subcircuit availability from metadata); rejection of `spice_model_card`, `lookup_table`, and `vendor_specific_artifact`; rejection of unsupported analyses (`dc_operating_point`, `dc_sweep`, `ac_small_signal`, `noise`, `thermal`) and models omitting `transient`; rejection of `unvalidated`, `failed_validation`, and version-mismatched `out_of_date` (`1.1.0` vs `1.0.0`) models; rejection of synthetic fixtures under `ExecutionPolicy::engineering_approved_only` and acceptance with `ExecutionClassification::test_only_synthetic` under `ExecutionPolicy::test_only_synthetic`; acceptance of non-synthetic assembly under `engineering_approved_only` and rejection of mixed synthetic/non-synthetic circuits. |
| WP3: Operating conditions & validity envelope | Rejection of out-of-envelope ambient temperature (`150 degC`, `-60 degC` outside `[-55, 125] degC`), out-of-envelope capacitor voltage (`60 V > 50 V`), out-of-envelope resistor continuous power ($10^2 / 1000 = 0.100\text{ W} > 0.050\text{ W}$), and out-of-envelope signal frequency (`5e7 Hz > 1e6 Hz`); rejection of missing units, `NaN` values, and unit mismatches against envelope constraints. |
| WP4 & WP6: Simulation adapter & manual `RcConfig` equivalence | Separation of `RcPulseStimulus`, `RcTransientControls`, and `RcSolverControls` from component parameters; rejection of unsupported solver overrides (`method = "gear"`, `temp = 50 degC`) and invalid RC timing (`step_s = 1e-3`); exact field, `config_json`, `rc_netlist`, netlist FNV-1a-64 hash (`fce4e70faf4e32d4`), 4001-point common grid, `analytical_voltage`, `ramp_voltage`, and `0.05 V` tolerance equality against manual `RcConfig{}`. |
| WP5: Provenance & real `ngspice-47` equivalence | Complete component, knowledge, model, `NormalizedParameterId`, `ObservationId`, `EvidenceId`, `SourceDocumentId`, `conversion_rule`, `OperatingCondition`, `TerminalNodeBinding`, `ExecutionClassification`, and FNV-1a-64 hash recording in `VerticalSliceProvenanceManifest` and `provenance_metadata.json`; fail-fast pre-execution input gate creating no directory on invalid input; real `ngspice-47` execution confirming byte-identical `circuit.cir`, `results.csv` (SHA-256 `9c923c4d86a96bdf42fe3bbd84a00cc6ae8519f5ac8ea3858b614c30260f26f9`), and `errors.csv` between `execute_vertical_rc_run` and manual `execute_rc_run`. |

## Post-implementation CTest and Phase 1.5 non-regression results

Executed in clean build directory `build/phase3-wp6`:

| CTest suite | Label | Result | Evidence |
| --- | --- | --- | --- |
| `unit` | `unit` | Passed (0.02 s) | `PASS: 92 unit checks` (unchanged Phase 1/1.5 unit checks). |
| `component_model` | `unit` | Passed (0.02 s) | `PASS: 270 component domain checks` (unchanged Phase 2 domain checks). |
| `vertical_slice` | `unit` | Passed (0.03 s) | `PASS: 1392 vertical slice checks` (Phase 3 WP1–WP6 unit checks). |
| `backend_errors` | `unit` | Passed (1.32 s) | Unchanged subprocess fault, timeout, concurrency, argv, and sentinel checks. |
| `ngspice_rc` | `integration` | Passed (0.28 s) | Real `ngspice-47` manual + vertical slice runs: 640 samples, native `max_error_V = 1.42982e-5`, common-grid `max_error_V = 6.31106e-5`, `rmse_V = 1.13392e-5`, byte-identical `results.csv`/`errors.csv`/`circuit.cir`, `provenance_metadata.json` verified, invalid netlist rejected with exit 1. |

Aggregate local CTest: **5/5 passed, 0 failed, 0 skipped**.

Additional Phase 1.5 non-regression checks run after Phase 3 implementation:

- `python -m unittest discover -s tests/ci -v`: **5/5 passed** (`OK`).
- `./build/phase3-wp6/rc_study.exe D:/projects/datasim/build/tools/msys2-ngspice/ucrt64/bin/ngspice_con.exe build/phase3-rc-study-check`:
  all six runs (10 us, 5 us, 2.5 us, 1.25 us refinements plus two 10 us repeats)
  passed with unchanged common-grid metrics (`6.31106e-5 V`, `1.78925e-5 V`,
  `6.00291e-6 V`, `2.66873e-6 V`), `0 V` repeat delta, and baseline `run-0/results.csv`
  SHA-256 `9c923c4d86a96bdf42fe3bbd84a00cc6ae8519f5ac8ea3858b614c30260f26f9`,
  matching both Phase 1.5 and the Phase 3 vertical-slice `results.csv` byte for byte.

## Limitations

- Only Windows x64 (MSYS2 UCRT GCC 16.2.0 / Ninja / ngspice-47) is verified locally
  in this session; Linux, macOS, and MSVC remain unverified, and hosted GitHub
  Actions CI for `feat/phase-3-vertical-slice` has not yet been triggered.
- Only ideal first-order RC transient simulation (`ModelFormat::analytical_equation`,
  `AnalysisType::transient`) is supported; external SPICE subcircuits, nonlinear
  semiconductor models, real vendor datasheet extraction, and physical hardware
  measurements (`MEASURED`) are not implemented.
