# Datasim

**Datasheet-driven electronic component modeling, simulation, and visualization.**

Datasim currently comprises three **VERIFIED** C++20 modules on Windows x64
(MSYS2 UCRT GCC 16.2.0):

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

This is not yet a general-purpose circuit simulator or datasheet processor. PDF/OCR
ingestion, persistent storage, general-purpose JSON serialization, automatic unit
conversion, LLM integration, external SPICE subcircuit loading, semiconductor models,
and interactive visualization remain **PROPOSED**.

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
- [Phase 4B implementation](docs/PHASE_4_IMPLEMENTATION.md) and [validation](docs/PHASE_4_VALIDATION.md) for the bounded in-memory candidate intake feature branch
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
