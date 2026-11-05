# Datasim

**Datasheet-driven electronic component modeling, simulation, and visualization.**

Datasim comprises three verified numerical/domain modules, the verified Phase 4B
candidate intake, and the verified Phase 4C bounded PDF table adapter on Windows x64:

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
8. **Phase 4F explicit column-header unit and condition binding (IMPLEMENTED and VERIFIED on `feat/phase-4f-explicit-header-binding`)**:
   caller-configured, source-verified `HeaderUnitBinding` and `HeaderConditionBinding` inside
   `datasim_pdf` with distinct header `EvidenceLocation` provenance, verified on synthetic
   regression cases A–H and locally benchmarked on the Phase 4E real-datasheet pilot corpus
   (`TP = 14/15`, `Precision = 0.9333`, `Recall = 0.9333`, `F1 = 0.9333`, `4/4` unsupported
   layouts fail-closed). See [Phase 4F plan](docs/PHASE_4F_PLAN.md),
   [Phase 4F implementation](docs/PHASE_4F_IMPLEMENTATION.md),
   [Phase 4F validation](docs/PHASE_4F_VALIDATION.md), and
   [ADR-0007](docs/decisions/ADR-0007-explicit-header-binding.md).

This is not yet a general-purpose circuit simulator or datasheet processor. Arbitrary
PDF layout recognition, OCR ingestion, persistent storage, general-purpose JSON
serialization, automatic unit conversion, LLM integration, external SPICE subcircuit
loading, semiconductor models, physical measurement validation (`MEASURED`), and
interactive visualization remain **PROPOSED**. Real-datasheet extraction is evaluated
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
