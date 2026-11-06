# Datasim

**Datasheet-driven electronic component modeling and simulation.**

Datasim has a verified ideal-RC simulation slice, in-memory component and model
contracts, candidate-only intake, and bounded selectable-text PDF extraction.
Phases 4A–4J are merged into `main`; Phase 4J closed via PR #14 at
`3fb583a0eb81640de421bc9049db9c0b80102e50`. Its validation and historical hosted
evidence are summarized in [Phase 4 final validation](docs/PHASE_4_FINAL_VALIDATION.md)
and the [release summary](docs/PHASE_4_RELEASE_SUMMARY.md). Phase 5A merged via
PR #15 at `483e4c7f4c43d5e78839dc92a4e1be59ae45660a`; post-merge Windows run
`42155239727` succeeded. Phase 5B extends the optional viewer with a manual
ideal-RC workbench on `feat/phase-5b-rc-simulation-workbench`; see its validation
record for local and hosted evidence.

1. **Phase 1 / 1.5 ideal-RC simulation spike (`datasim`)**: manual ideal-RC
   configuration, ngspice-47 batch execution, CSV/JSON run reporting, common-grid
   convergence/repeatability validation, and process fault hardening, merged into
   `main` via PR #1 (`86b924b4b49d69c70c1688a8748cb1c2893adc34`) and verified
   locally and in [hosted Windows CI](https://github.com/quocthaile/datasim/actions/runs/41217248663).
   See [Phase 1 implementation](docs/PHASE_1_IMPLEMENTATION.md),
   [Phase 1/1.5 validation](docs/PHASE_1_VALIDATION.md),
   [reproducibility](docs/PHASE_1_REPRODUCIBILITY.md), and the
   [architecture review](docs/PHASE_1_ARCHITECTURE_REVIEW.md).
2. **Phase 2 component domain model & knowledge contracts (`datasim_component`)**:
   dependency-free in-memory C++20 representations for component identity, source
   documents, evidence locations, single-qualifier parameter observations,
   operating conditions, deterministic human-gated review lifecycle, versioned
   component knowledge with visible conflicts, model artifact descriptors,
   independent model validation records, and referential integrity rules, merged into
   `main` via PR #2 (`b2f73d5`, 270 domain unit checks). See
   [Phase 2 plan](docs/PHASE_2_PLAN.md),
   [Phase 2 implementation](docs/PHASE_2_IMPLEMENTATION.md),
   [Phase 2 validation](docs/PHASE_2_VALIDATION.md), and
   [ADR-0003](docs/decisions/ADR-0003-component-domain-model.md).
3. **Phase 3 deterministic component-to-simulation vertical slice (`datasim_vertical`)**:
   application-level multi-bundle `CircuitAssembly`, SI parameter resolution
   (`resolve_rc_parameters`), fail-closed model/backend compatibility and
   `ExecutionPolicy` metadata gate (`evaluate_rc_model_compatibility`), ideal-RC
   simulation adapter (`adapt_vertical_rc_simulation`), and traceable
   `provenance_metadata.json` run reporting (`execute_vertical_rc_run`), merged into
   `main` at `6516a629c75662b83f48704bfd7978bcc299ff78` (verified in
   [hosted Windows CI run 41284202716](https://github.com/quocthaile/datasim/actions/runs/41284202716)
   with 1392 unit checks, real `ngspice-47` integration, and six-run study) and
   hardened via PR #3 (merge `c142a36`, hosted CI run 41766031217 `success`; 1483 vertical-slice unit checks plus
   byte-identical real `ngspice-47` integration against manual `RcConfig`). See
   [Phase 3 plan](docs/PHASE_3_PLAN.md),
   [Phase 3 implementation](docs/PHASE_3_IMPLEMENTATION.md),
   [Phase 3 validation](docs/PHASE_3_VALIDATION.md), and
   [ADR-0004](docs/decisions/ADR-0004-phase-3-vertical-slice.md).
4. **Phase 4B deterministic evidence intake (`datasim_intake`)**: candidate-only
   manifests, strict validation, raw numeric handling and validated handoff to the
   component model. Merged by PR #6 at
   `7d9af1c88929fe3c29d55e1c2438e8d2041acb2a`; hosted Windows CI run
   [41995104973](https://github.com/quocthaile/datasim/actions/runs/41995104973)
   exercised the non-skipped P-07 real-ngspice path.
5. **Phase 4C deterministic PDF table extraction (`datasim_pdf`)**: isolated
   Poppler `pdfinfo` / `pdftotext -bbox-layout` adapter for explicitly selected
   single-page tables with non-overlapping column spans, emitting candidate-only
   observations through `datasim_intake`. Merged by PR #7 at
   `1704f726d901f3e1d00f9cdc918e80d352b443ac` and verified on synthetic fixtures in
   hosted Windows CI runs
   [42000652152](https://github.com/quocthaile/datasim/actions/runs/42000652152) (PR #7)
   and [42017272043](https://github.com/quocthaile/datasim/actions/runs/42017272043)
   (post-merge `main`). See [Phase 4C plan](docs/PHASE_4C_PLAN.md),
   [Phase 4C implementation](docs/PHASE_4C_IMPLEMENTATION.md),
   [Phase 4C validation](docs/PHASE_4C_VALIDATION.md), and
   [ADR-0006](docs/decisions/ADR-0006-deterministic-pdf-table-extraction.md).
6. **Phase 4D PDF-to-Simulation end-to-end validation gate**:
   end-to-end integration test (`tests/integration/pdf_simulation_gate.cpp`) connecting
   real Poppler extraction (`datasim_pdf`) on `synthetic_datasheet.pdf` (`SIM-R-10K` and
   `SIM-C-1UF` at `T_A=85 degC`) through `datasim_intake`, synthetic engineering review and
   conflict resolution (`datasim_component`), model compatibility and assembly
   (`datasim_vertical`), and real `ngspice-47` simulation (`datasim`), verified against an
   equivalent manual `RcConfig` (`R=10000 ohm`, `C=1e-6 F`) with 768 positive and negative
   trust-boundary checks. Merged into `main` via PR #8 at
   `69aba4df7ff752355a40e394de5d05ac988df34b` and verified in hosted Windows CI runs
   [42023680012](https://github.com/quocthaile/datasim/actions/runs/42023680012) (PR #8) and
   [42025406013](https://github.com/quocthaile/datasim/actions/runs/42025406013) (post-merge `main`).
   See [Phase 4D plan](docs/PHASE_4D_PLAN.md),
   [Phase 4D implementation](docs/PHASE_4D_IMPLEMENTATION.md), and
   [Phase 4D validation](docs/PHASE_4D_VALIDATION.md).
7. **Phase 4E real-datasheet evaluation corpus and offline benchmark**:
   versioned corpus protocol, ground-truth annotation schema, development-only C++ extraction
   dump adapter (`datasheet_eval_dump`), offline corpus evaluator (`scripts/eval/evaluate_pdf_corpus.py`),
   and automated determinism/regression suite (`pdf_corpus_evaluation`), merged into `main` via
   PR #9 at `b45c32224e0316dd9f93b6e9bfc3ab89cd3ceaf4` and verified in hosted Windows CI runs
   [42031281489](https://github.com/quocthaile/datasim/actions/runs/42031281489),
   [42031409931](https://github.com/quocthaile/datasim/actions/runs/42031409931) (PR #9), and
   [42037281172](https://github.com/quocthaile/datasim/actions/runs/42037281172) (post-merge `main`).
   See [Phase 4E plan](docs/PHASE_4E_PLAN.md),
   [Phase 4E corpus protocol](docs/PHASE_4E_CORPUS_PROTOCOL.md), and
   [Phase 4E validation](docs/PHASE_4E_VALIDATION.md).
8. **Phase 4F explicit column-header unit and condition binding**:
   caller-configured, source-verified `HeaderUnitBinding` and `HeaderConditionBinding` inside
   `datasim_pdf` with distinct header `EvidenceLocation` provenance, merged into `main` via
   PR #10 at `9f3c115d13a40204d3d7f83382a50c107b14b803`, verified in hosted Windows CI runs
   [42042283386](https://github.com/quocthaile/datasim/actions/runs/42042283386),
   [42043481659](https://github.com/quocthaile/datasim/actions/runs/42043481659) (PR #10), and
   [42044651168](https://github.com/quocthaile/datasim/actions/runs/42044651168) (post-merge `main`),
   and locally benchmarked on the Phase 4E real-datasheet pilot corpus (`TP = 14/15`,
   `Precision = 0.9333`, `Recall = 0.9333`, `F1 = 0.9333`, `4/4` unsupported layouts
   fail-closed). See [Phase 4F plan](docs/PHASE_4F_PLAN.md),
   [Phase 4F implementation](docs/PHASE_4F_IMPLEMENTATION.md),
   [Phase 4F validation](docs/PHASE_4F_VALIDATION.md), and
   [ADR-0007](docs/decisions/ADR-0007-explicit-header-binding.md).
9. **Phase 4G deterministic inline value/unit parsing (merged via PR #11 at `357dc52aa809`; post-merge Windows CI `42052973122`)**:
   caller-configured `InlineValueUnitBinding` and `ValueCellProvenance` inside `datasim_pdf`
   for single-cell numeric value and unit tokens (`200 V`, `0.4 W`, `220 pF`, `10 nF`, `1.0 µF`)
   with strict unit allowlists, unit precedence/conflict rules, intra-cell word bounding-box
   collision rejection, and verbatim raw-cell preservation (`raw_cell_text`), verified on
   synthetic regression cases 1–14 and locally benchmarked on the real-datasheet pilot corpus
   (`TP = 14/15` on the identical 15-observation subset eliminating sub-point column splits, and
   `TP = 20/22`, `F1 = 0.9091` on the P2-expanded 22-observation subset). See
   [Phase 4G plan](docs/PHASE_4G_PLAN.md),
   [Phase 4G implementation](docs/PHASE_4G_IMPLEMENTATION.md),
   [Phase 4G validation](docs/PHASE_4G_VALIDATION.md), and
   [ADR-0008](docs/decisions/ADR-0008-inline-value-unit-parsing.md).

10. **Phase 4H explicit hierarchical context**: source-verified parent headings and
   narrowly bounded merged parameter cells, with exact child selection and separate
   parent/child provenance. Merged via PR #12 at `864eb56`, verified by post-merge
   Windows CI `42058944651`;
   see [implementation](docs/PHASE_4H_IMPLEMENTATION.md),
   [validation](docs/PHASE_4H_VALIDATION.md), and
   [ADR-0009](docs/decisions/ADR-0009-hierarchical-table-context.md).

11. **Phase 4I baseline-aware text reconstruction**: opt-in, source-verified
   host/subscript/superscript association with original word geometry and separate
   display provenance. Merged via PR #13 at
   `fb35ae1fc5a58dcd6b9f89d7a6807c3198ab99a5`; post-merge Windows CI run
   `42065262568` succeeded. See [implementation](docs/PHASE_4I_IMPLEMENTATION.md),
   [validation](docs/PHASE_4I_VALIDATION.md), and
   [ADR-0010](docs/decisions/ADR-0010-baseline-aware-text-reconstruction.md).
   Numerical interpretation is unchanged.

12. **Phase 4J final validation and release closure**: integrated architecture,
   numerical, PDF, corpus, provenance, rights, and documentation review on
   `chore/phase-4j-final-validation`, including an isolated PDF temporary-directory
   collision fix with regression coverage. Local Windows Release validation passed;
   hosted Windows run `42148338325` passed on commit `c631abc5f578a269b8748b39c631b7aa06b87768`; validation is VERIFIED. PR #14 merged into `main` at `3fb583a0eb81640de421bc9049db9c0b80102e50`. Treat
   Phase 4J as VERIFIED beyond the documented scope. See
   [final validation](docs/PHASE_4_FINAL_VALIDATION.md) and
   [release summary](docs/PHASE_4_RELEASE_SUMMARY.md).

13. **Phase 5A desktop visualization foundation**: optional `datasim_viewer`,
   using Qt 6 Widgets and existing `datasim::read_csv()` / `datasim::Samples`
   for read-only waveform display. Merged via PR #15 at
   `483e4c7f4c43d5e78839dc92a4e1be59ae45660a`; hosted post-merge Windows run
   `42155239727` succeeded. See [Phase 5A validation](docs/PHASE_5A_VALIDATION.md).
14. **Phase 5B interactive ideal-RC workbench**: manual `RcConfig` entry,
   `datasim::check_config()` validation, asynchronous `execute_rc_run()`, and
   read-only display of returned waveform, metrics, and run artifacts. Implemented
   on `feat/phase-5b-rc-simulation-workbench`; local and hosted evidence is in
   [Phase 5B validation](docs/PHASE_5B_VALIDATION.md). No physical measurement or
   datasheet-approved simulation is claimed.

This is not yet a general-purpose circuit simulator or datasheet processor. Arbitrary
PDF layout recognition, OCR ingestion, persistent storage, general-purpose JSON
serialization, automatic unit conversion, LLM integration, external SPICE subcircuit
loading, semiconductor models, physical measurement validation (`MEASURED`), and
general visualization workflows remain **PROPOSED**. Phase 5A is merged and
Phase 5B is under review; see their validation records for verified functions.
Real-datasheet extraction is evaluated
offline only within the bounded Phase 4E pilot corpus (`docs/PHASE_4E_VALIDATION.md`);
general multi-layout vendor datasheet accuracy is not claimed.

The intended workflow moves from source evidence to reviewed component knowledge,
then to an explicitly selected model and deterministic simulation. A datasheet
parameter is not automatically a complete mathematical model. Local LLMs may
assist document work and explanations in future phases; they are never the
numerical or review authority.

## Documentation

- [Project charter and status definitions](docs/PROJECT_CHARTER.md)
- [Architecture and trust boundaries](docs/ARCHITECTURE.md)
- [Component data model](docs/COMPONENT_DATA_MODEL.md)
- [Validation strategy](docs/VALIDATION_STRATEGY.md)
- [Roadmap and open questions](docs/ROADMAP.md)
- [Phase 1 implementation](docs/PHASE_1_IMPLEMENTATION.md), [validation](docs/PHASE_1_VALIDATION.md), [reproducibility](docs/PHASE_1_REPRODUCIBILITY.md), and [architecture review](docs/PHASE_1_ARCHITECTURE_REVIEW.md)
- [Phase 2 plan](docs/PHASE_2_PLAN.md), [implementation](docs/PHASE_2_IMPLEMENTATION.md), and [validation](docs/PHASE_2_VALIDATION.md)
- [Phase 3 plan](docs/PHASE_3_PLAN.md), [implementation](docs/PHASE_3_IMPLEMENTATION.md), and [validation](docs/PHASE_3_VALIDATION.md)
- [Phase 4 plan](docs/PHASE_4_PLAN.md) and [validation plan](docs/PHASE_4_VALIDATION_PLAN.md) for the Phase 4A design and reconciled Phase 4B test semantics
- [Phase 4B implementation](docs/PHASE_4_IMPLEMENTATION.md) and [validation](docs/PHASE_4_VALIDATION.md) for the merged, hosted-verified in-memory candidate intake
- [Phase 4C plan](docs/PHASE_4C_PLAN.md), [implementation](docs/PHASE_4C_IMPLEMENTATION.md), [validation](docs/PHASE_4C_VALIDATION.md), and [ADR-0006](docs/decisions/ADR-0006-deterministic-pdf-table-extraction.md) for the bounded local PDF table adapter
- [Phase 4D plan](docs/PHASE_4D_PLAN.md), [implementation](docs/PHASE_4D_IMPLEMENTATION.md), and [validation](docs/PHASE_4D_VALIDATION.md) for the PDF-to-Simulation end-to-end validation gate
- [Phase 4E plan](docs/PHASE_4E_PLAN.md), [corpus protocol](docs/PHASE_4E_CORPUS_PROTOCOL.md), and [validation](docs/PHASE_4E_VALIDATION.md) for the real-datasheet evaluation corpus and offline benchmark
- [Phase 4F plan](docs/PHASE_4F_PLAN.md), [implementation](docs/PHASE_4F_IMPLEMENTATION.md), [validation](docs/PHASE_4F_VALIDATION.md), and [ADR-0007](docs/decisions/ADR-0007-explicit-header-binding.md) for explicit column-header unit and condition bindings
- [Phase 4G plan](docs/PHASE_4G_PLAN.md), [implementation](docs/PHASE_4G_IMPLEMENTATION.md), [validation](docs/PHASE_4G_VALIDATION.md), and [ADR-0008](docs/decisions/ADR-0008-inline-value-unit-parsing.md) for deterministic inline value/unit parsing
- [Phase 4H implementation](docs/PHASE_4H_IMPLEMENTATION.md), [validation](docs/PHASE_4H_VALIDATION.md), and [ADR-0009](docs/decisions/ADR-0009-hierarchical-table-context.md) for explicit hierarchical context
- [Phase 4I implementation](docs/PHASE_4I_IMPLEMENTATION.md), [validation](docs/PHASE_4I_VALIDATION.md), and [ADR-0010](docs/decisions/ADR-0010-baseline-aware-text-reconstruction.md) for explicit baseline-aware text reconstruction
- [Phase 4 final validation](docs/PHASE_4_FINAL_VALIDATION.md) and [release summary](docs/PHASE_4_RELEASE_SUMMARY.md) for release closure
- [Phase 5A plan](docs/PHASE_5A_PLAN.md), [architecture](docs/PHASE_5A_ARCHITECTURE.md), [validation](docs/PHASE_5A_VALIDATION.md), and [ADR-0011](docs/decisions/ADR-0011-desktop-visualization-foundation.md) for the optional read-only waveform viewer
- [Phase 5B plan](docs/PHASE_5B_PLAN.md), [architecture](docs/PHASE_5B_ARCHITECTURE.md), [implementation](docs/PHASE_5B_IMPLEMENTATION.md), [validation](docs/PHASE_5B_VALIDATION.md), and [ADR-0012](docs/decisions/ADR-0012-interactive-rc-workbench.md) for the optional manual ideal-RC workbench
- [Annotated research references](docs/RESEARCH_REFERENCES.md) and [BibTeX](docs/REFERENCES.bib)
- [Glossary](docs/GLOSSARY.md)
- [ADR-0001: simulation backend boundary](docs/decisions/ADR-0001-simulation-backend-boundary.md)
- [ADR-0002: Phase 1 executable integration](docs/decisions/ADR-0002-phase-1-ngspice-executable.md)
- [ADR-0003: Phase 2 component domain model](docs/decisions/ADR-0003-component-domain-model.md)
- [ADR-0004: Phase 3 vertical slice](docs/decisions/ADR-0004-phase-3-vertical-slice.md)
- [ADR-0005: Evidence intake boundary (PROPOSED)](docs/decisions/ADR-0005-evidence-intake-boundary.md)
- [Agent instructions](AGENTS.md) and [repository initialization prompt](docs/AGENT_INIT_REPOSITORY.md)

## Contributing at this stage

Keep changes scoped to the authorized phase. Preserve evidence, units, operating
conditions, exact part suffixes, and review history. Record technology choices in
decision records before adopting them. Phase 1 selects C++20, CMake/CTest and
ngspice executable integration; Phase 2 uses standard-library C++20 only for
in-memory domain contracts. UI toolkit, graphics stack, storage, and LLM runtime
remain undecided.

The original `.gitignore` is unchanged; generated runs and local tools belong under `build/`.
No license file is present. Project licensing remains an owner decision; do not
assume redistribution rights for this repository or third-party assets. Link to
datasheets and vendor models instead of copying them into the repository.
